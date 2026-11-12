#include "apexcine/l3_container/ApexRawContainer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

namespace apexcine::l3 {

namespace {
constexpr std::array<uint32_t, 256> makeCrc32cTable() noexcept {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0x82F63B78u) : (crc >> 1);
        }
        table[i] = crc;
    }
    return table;
}

constexpr auto kCrc32cTable = makeCrc32cTable();
constexpr size_t kStagingBufferCapacity = 2 * 1024 * 1024; // 2 MiB sequential write slab
} // namespace

uint32_t computeCrc32c(const void* data, size_t length, uint32_t initial) noexcept {
    uint32_t crc = ~initial;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) {
        crc = kCrc32cTable[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

ApexRawContainerWriter::~ApexRawContainerWriter() noexcept {
    if (fd_ >= 0) {
        (void)finalizeAndClose();
    }
}

bool ApexRawContainerWriter::openAndPreallocate(const std::string& filePath,
                                                const AcrawFileHeader& header,
                                                uint64_t preallocateBytes) noexcept {
    if (fd_ >= 0) {
        (void)finalizeAndClose();
    }

    fd_ = ::open(filePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_ < 0) {
        return false;
    }

#if defined(_GNU_SOURCE) || defined(__linux__)
    if (preallocateBytes > 0) {
        (void)::posix_fallocate(fd_, 0, static_cast<off_t>(preallocateBytes));
    }
#endif

    writeStagingBuffer_.resize(kStagingBufferCapacity);
    stagingUsed_ = 0;
    currentFileOffset_ = 0;
    diskFlushedOffset_ = 0;
    frameIndex_.clear();
    frameIndex_.reserve(4096);
    lastCheckpointedFrameIdx_ = 0;
    checkpointsWritten_ = 0;
    checkpointInterval_ = std::max<uint16_t>(1, header.journalCheckpointIntervalFrames);

    AcrawFileHeader signedHeader = header;
    signedHeader.magic[0] = 'A';
    signedHeader.magic[1] = 'C';
    signedHeader.magic[2] = 'R';
    signedHeader.magic[3] = 'A';
    signedHeader.magic[4] = 'W';
    signedHeader.magic[5] = 0x01;
    signedHeader.magic[6] = 0x00;
    signedHeader.magic[7] = 0x00;
    signedHeader.headerCrc32c =
        computeCrc32c(&signedHeader, sizeof(AcrawFileHeader) - sizeof(uint32_t));

    return appendRawBytes(&signedHeader, sizeof(signedHeader));
}

bool ApexRawContainerWriter::appendRawBytes(const void* data, size_t len) noexcept {
    const auto* src = static_cast<const uint8_t*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        const size_t space = writeStagingBuffer_.size() - stagingUsed_;
        const size_t toCopy = std::min(remaining, space);
        std::memcpy(writeStagingBuffer_.data() + stagingUsed_, src, toCopy);
        stagingUsed_ += toCopy;
        src += toCopy;
        remaining -= toCopy;
        currentFileOffset_ += toCopy;

        if (stagingUsed_ == writeStagingBuffer_.size()) {
            if (!flushWriteBufferToDisk()) {
                return false;
            }
        }
    }
    return true;
}

bool ApexRawContainerWriter::flushWriteBufferToDisk() noexcept {
    if (stagingUsed_ == 0 || fd_ < 0) {
        return true;
    }
    struct iovec iov {
        writeStagingBuffer_.data(), stagingUsed_
    };
    const ssize_t written = ::pwritev(fd_, &iov, 1, static_cast<off_t>(diskFlushedOffset_));
    if (written != static_cast<ssize_t>(stagingUsed_)) {
        return false;
    }
    diskFlushedOffset_ += stagingUsed_;
    stagingUsed_ = 0;
    return true;
}

bool ApexRawContainerWriter::writeJournalCheckpointChunk() noexcept {
    // Write ONLY the delta frame index entries since lastCheckpointedFrameIdx_ (O(1) per checkpoint, O(N) total)
    const size_t deltaCount = frameIndex_.size() - lastCheckpointedFrameIdx_;
    const AcrawFrameIndexEntry* deltaPtr = frameIndex_.data() + lastCheckpointedFrameIdx_;
    const size_t payloadBytes = deltaCount * sizeof(AcrawFrameIndexEntry);

    AcrawChunkHeader ch{};
    ch.chunkType = static_cast<uint32_t>(AcrawChunkType::JOURNAL_INDEX_CKPT);
    ch.payloadBytes = static_cast<uint32_t>(payloadBytes);
    ch.sequenceNumber = frameIndex_.size();
    ch.timestampNs = frameIndex_.empty() ? 0 : frameIndex_.back().timestampNs;
    ch.payloadCrc32c = computeCrc32c(deltaPtr, payloadBytes);
    ch.headerCrc32c = computeCrc32c(&ch, sizeof(AcrawChunkHeader) - sizeof(uint32_t));

    if (!appendRawBytes(&ch, sizeof(ch))) return false;
    if (payloadBytes > 0 && !appendRawBytes(deltaPtr, payloadBytes)) return false;
    if (!flushWriteBufferToDisk()) return false;

    lastCheckpointedFrameIdx_ = frameIndex_.size();
    ++checkpointsWritten_;
    // Periodic fdatasync every 4 checkpoints (~2.6s @ 24fps) to bound power-loss window without per-frame stalls
    if ((checkpointsWritten_ % 4u) == 0u && fd_ >= 0) {
        (void)::fdatasync(fd_);
    }
    return true;
}

bool ApexRawContainerWriter::writeVideoFrame(uint64_t frameSequence,
                                             int64_t timestampNs,
                                             const AcrawFrameMetadataPrefix& meta,
                                             const uint8_t* compressedBitstream,
                                             size_t bitstreamBytes) noexcept {
    if (fd_ < 0 || compressedBitstream == nullptr || bitstreamBytes == 0) {
        return false;
    }

    const uint64_t chunkStartOffset = currentFileOffset_;
    const uint32_t totalPayloadBytes =
        static_cast<uint32_t>(sizeof(AcrawFrameMetadataPrefix) + bitstreamBytes);

    uint32_t payloadCrc = computeCrc32c(&meta, sizeof(meta), 0);
    payloadCrc = computeCrc32c(compressedBitstream, bitstreamBytes, payloadCrc);

    AcrawChunkHeader ch{};
    ch.chunkType = static_cast<uint32_t>(AcrawChunkType::RAW_VIDEO_FRAME);
    ch.payloadBytes = totalPayloadBytes;
    ch.sequenceNumber = frameSequence;
    ch.timestampNs = timestampNs;
    ch.payloadCrc32c = payloadCrc;
    ch.headerCrc32c = computeCrc32c(&ch, sizeof(AcrawChunkHeader) - sizeof(uint32_t));

    if (!appendRawBytes(&ch, sizeof(ch))) return false;
    if (!appendRawBytes(&meta, sizeof(meta))) return false;
    if (!appendRawBytes(compressedBitstream, bitstreamBytes)) return false;

    AcrawFrameIndexEntry idx{};
    idx.frameSequenceNumber = frameSequence;
    idx.timestampNs = timestampNs;
    idx.fileOffsetBytes = chunkStartOffset;
    idx.chunkPayloadBytes = totalPayloadBytes;
    idx.frameCrc32c = payloadCrc;
    frameIndex_.push_back(idx);

    if ((frameIndex_.size() % checkpointInterval_) == 0) {
        if (!writeJournalCheckpointChunk()) {
            return false;
        }
    }
    return true;
}

bool ApexRawContainerWriter::writeImuBatch(uint64_t batchSeq,
                                           int64_t timestampNs,
                                           const l1::ImuSample* samples,
                                           size_t sampleCount) noexcept {
    if (fd_ < 0 || samples == nullptr || sampleCount == 0) return false;
    const size_t payloadBytes = sampleCount * sizeof(l1::ImuSample);
    AcrawChunkHeader ch{};
    ch.chunkType = static_cast<uint32_t>(AcrawChunkType::IMU_GYRO_ACCEL);
    ch.payloadBytes = static_cast<uint32_t>(payloadBytes);
    ch.sequenceNumber = batchSeq;
    ch.timestampNs = timestampNs;
    ch.payloadCrc32c = computeCrc32c(samples, payloadBytes);
    ch.headerCrc32c = computeCrc32c(&ch, sizeof(AcrawChunkHeader) - sizeof(uint32_t));

    return appendRawBytes(&ch, sizeof(ch)) && appendRawBytes(samples, payloadBytes);
}

bool ApexRawContainerWriter::writeAudioPcmFloat(uint64_t audioSeq,
                                                int64_t timestampNs,
                                                const float* interleavedStereoFloat,
                                                size_t floatSampleCount) noexcept {
    if (fd_ < 0 || interleavedStereoFloat == nullptr || floatSampleCount == 0) return false;
    const size_t payloadBytes = floatSampleCount * sizeof(float);
    AcrawChunkHeader ch{};
    ch.chunkType = static_cast<uint32_t>(AcrawChunkType::PCM_FLOAT_AUDIO);
    ch.payloadBytes = static_cast<uint32_t>(payloadBytes);
    ch.sequenceNumber = audioSeq;
    ch.timestampNs = timestampNs;
    ch.payloadCrc32c = computeCrc32c(interleavedStereoFloat, payloadBytes);
    ch.headerCrc32c = computeCrc32c(&ch, sizeof(AcrawChunkHeader) - sizeof(uint32_t));

    return appendRawBytes(&ch, sizeof(ch)) &&
           appendRawBytes(interleavedStereoFloat, payloadBytes);
}

bool ApexRawContainerWriter::finalizeAndClose() noexcept {
    if (fd_ < 0) return false;

    const size_t payloadBytes = frameIndex_.size() * sizeof(AcrawFrameIndexEntry);
    AcrawChunkHeader footer{};
    footer.chunkType = static_cast<uint32_t>(AcrawChunkType::FINAL_FOOTER_INDEX);
    footer.payloadBytes = static_cast<uint32_t>(payloadBytes);
    footer.sequenceNumber = frameIndex_.size();
    footer.timestampNs = frameIndex_.empty() ? 0 : frameIndex_.back().timestampNs;
    footer.payloadCrc32c = computeCrc32c(frameIndex_.data(), payloadBytes);
    footer.headerCrc32c = computeCrc32c(&footer, sizeof(AcrawChunkHeader) - sizeof(uint32_t));

    bool ok = appendRawBytes(&footer, sizeof(footer));
    if (ok && payloadBytes > 0) {
        ok = appendRawBytes(frameIndex_.data(), payloadBytes);
    }
    if (ok) {
        ok = flushWriteBufferToDisk();
    }
    if (ok) {
        (void)::ftruncate(fd_, static_cast<off_t>(diskFlushedOffset_));
        (void)::fdatasync(fd_);
    }
    ::close(fd_);
    fd_ = -1;
    return ok;
}

void ApexRawContainerWriter::abortSimulatePowerLossMidTake() noexcept {
    if (fd_ >= 0) {
        (void)flushWriteBufferToDisk();
        // Do NOT write FINAL_FOOTER_INDEX and do NOT truncate preallocated tail or torn chunk
        ::close(fd_);
        fd_ = -1;
    }
}

ContainerRecoveryReport ApexRawContainerReader::openAndRecover(const std::string& filePath) {
    ContainerRecoveryReport report{};
    const int fd = ::open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        return report;
    }

    struct stat st {};
    if (::fstat(fd, &st) != 0 || static_cast<size_t>(st.st_size) < sizeof(AcrawFileHeader)) {
        ::close(fd);
        return report;
    }
    const uint64_t fileSize = static_cast<uint64_t>(st.st_size);

    AcrawFileHeader hdr{};
    if (::pread(fd, &hdr, sizeof(hdr), 0) != sizeof(hdr)) {
        ::close(fd);
        return report;
    }

    const uint32_t expectedHdrCrc =
        computeCrc32c(&hdr, sizeof(AcrawFileHeader) - sizeof(uint32_t));
    if (hdr.magic[0] != 'A' || hdr.magic[1] != 'C' || hdr.magic[2] != 'R' ||
        hdr.magic[3] != 'A' || hdr.magic[4] != 'W' || hdr.headerCrc32c != expectedHdrCrc) {
        ::close(fd);
        return report;
    }
    report.fileHeader = hdr;

    uint64_t offset = sizeof(AcrawFileHeader);
    bool sawCleanFooter = false;
    std::vector<uint8_t> payloadBuf;

    while (offset + sizeof(AcrawChunkHeader) <= fileSize) {
        AcrawChunkHeader ch{};
        if (::pread(fd, &ch, sizeof(ch), static_cast<off_t>(offset)) != sizeof(ch)) {
            break;
        }
        const uint32_t expectedChCrc =
            computeCrc32c(&ch, sizeof(AcrawChunkHeader) - sizeof(uint32_t));
        if (ch.headerCrc32c != expectedChCrc ||
            offset + sizeof(AcrawChunkHeader) + ch.payloadBytes > fileSize) {
            // Reached uninitialized fallocate region or torn tail chunk from power loss
            report.corruptedTailBytesTruncated = static_cast<size_t>(fileSize - offset);
            break;
        }

        payloadBuf.resize(ch.payloadBytes);
        if (ch.payloadBytes > 0) {
            if (::pread(fd,
                        payloadBuf.data(),
                        ch.payloadBytes,
                        static_cast<off_t>(offset + sizeof(AcrawChunkHeader))) !=
                static_cast<ssize_t>(ch.payloadBytes)) {
                report.corruptedTailBytesTruncated = static_cast<size_t>(fileSize - offset);
                break;
            }
        }
        const uint32_t actualPayloadCrc = computeCrc32c(payloadBuf.data(), ch.payloadBytes);
        if (actualPayloadCrc != ch.payloadCrc32c) {
            report.corruptedTailBytesTruncated = static_cast<size_t>(fileSize - offset);
            break;
        }

        const auto type = static_cast<AcrawChunkType>(ch.chunkType);
        if (type == AcrawChunkType::RAW_VIDEO_FRAME) {
            AcrawFrameIndexEntry entry{};
            entry.frameSequenceNumber = ch.sequenceNumber;
            entry.timestampNs = ch.timestampNs;
            entry.fileOffsetBytes = offset;
            entry.chunkPayloadBytes = ch.payloadBytes;
            entry.frameCrc32c = ch.payloadCrc32c;
            report.frameIndex.push_back(entry);
            ++report.totalValidFramesRecovered;
        } else if (type == AcrawChunkType::IMU_GYRO_ACCEL) {
            ++report.totalImuBatchesRecovered;
        } else if (type == AcrawChunkType::PCM_FLOAT_AUDIO) {
            ++report.totalAudioChunksRecovered;
        } else if (type == AcrawChunkType::JOURNAL_INDEX_CKPT) {
            ++report.journalCheckpointsFound;
        } else if (type == AcrawChunkType::FINAL_FOOTER_INDEX) {
            sawCleanFooter = true;
        }

        offset += sizeof(AcrawChunkHeader) + ch.payloadBytes;
    }

    ::close(fd);
    report.lastValidChunkEndOffset = offset;
    report.openedCleanly = sawCleanFooter && (report.corruptedTailBytesTruncated == 0);
    report.wasRecoveredFromCrash = !sawCleanFooter && !report.frameIndex.empty();
    return report;
}

bool ApexRawContainerReader::repairAndFinalizeTruncatedFile(const std::string& filePath) {
    ContainerRecoveryReport rec = openAndRecover(filePath);
    if (rec.frameIndex.empty() || rec.lastValidChunkEndOffset < sizeof(AcrawFileHeader)) {
        return false;
    }
    if (rec.openedCleanly) {
        return true;
    }

    const int fd = ::open(filePath.c_str(), O_RDWR);
    if (fd < 0) return false;

    if (::ftruncate(fd, static_cast<off_t>(rec.lastValidChunkEndOffset)) != 0) {
        ::close(fd);
        return false;
    }

    const size_t payloadBytes = rec.frameIndex.size() * sizeof(AcrawFrameIndexEntry);
    AcrawChunkHeader footer{};
    footer.chunkType = static_cast<uint32_t>(AcrawChunkType::FINAL_FOOTER_INDEX);
    footer.payloadBytes = static_cast<uint32_t>(payloadBytes);
    footer.sequenceNumber = rec.frameIndex.size();
    footer.timestampNs = rec.frameIndex.back().timestampNs;
    footer.payloadCrc32c = computeCrc32c(rec.frameIndex.data(), payloadBytes);
    footer.headerCrc32c = computeCrc32c(&footer, sizeof(AcrawChunkHeader) - sizeof(uint32_t));

    struct iovec iovs[2] = {
        {&footer, sizeof(footer)},
        {rec.frameIndex.data(), payloadBytes}
    };
    const ssize_t expected = static_cast<ssize_t>(sizeof(footer) + payloadBytes);
    const ssize_t written = ::pwritev(fd, iovs, 2, static_cast<off_t>(rec.lastValidChunkEndOffset));
    (void)::fdatasync(fd);
    ::close(fd);
    return written == expected;
}

bool ApexRawContainerReader::readAndVerifyVideoFrame(
    const std::string& filePath,
    const AcrawFrameIndexEntry& entry,
    AcrawFrameMetadataPrefix& outMeta,
    std::vector<uint8_t>& outCompressedBitstream) {
    const int fd = ::open(filePath.c_str(), O_RDONLY);
    if (fd < 0) return false;

    AcrawChunkHeader ch{};
    if (::pread(fd, &ch, sizeof(ch), static_cast<off_t>(entry.fileOffsetBytes)) != sizeof(ch)) {
        ::close(fd);
        return false;
    }

    const uint32_t expectedChCrc =
        computeCrc32c(&ch, sizeof(AcrawChunkHeader) - sizeof(uint32_t));
    if (ch.headerCrc32c != expectedChCrc ||
        ch.chunkType != static_cast<uint32_t>(AcrawChunkType::RAW_VIDEO_FRAME) ||
        ch.payloadBytes < sizeof(AcrawFrameMetadataPrefix)) {
        ::close(fd);
        return false;
    }

    std::vector<uint8_t> payload(ch.payloadBytes);
    if (::pread(fd,
                payload.data(),
                ch.payloadBytes,
                static_cast<off_t>(entry.fileOffsetBytes + sizeof(AcrawChunkHeader))) !=
        static_cast<ssize_t>(ch.payloadBytes)) {
        ::close(fd);
        return false;
    }
    ::close(fd);

    if (computeCrc32c(payload.data(), payload.size()) != ch.payloadCrc32c) {
        return false;
    }

    std::memcpy(&outMeta, payload.data(), sizeof(AcrawFrameMetadataPrefix));
    const size_t bitstreamBytes = ch.payloadBytes - sizeof(AcrawFrameMetadataPrefix);
    outCompressedBitstream.assign(payload.begin() + sizeof(AcrawFrameMetadataPrefix), payload.end());
    return outCompressedBitstream.size() == bitstreamBytes;
}

} // namespace apexcine::l3
