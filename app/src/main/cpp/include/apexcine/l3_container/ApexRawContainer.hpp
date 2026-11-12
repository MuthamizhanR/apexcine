#pragma once

#include "apexcine/l0_runtime/BufferContract.hpp"
#include "apexcine/l1_capture/NdkCameraCaptureEngine.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace apexcine::l3 {

enum class AcrawChunkType : uint32_t {
    STREAM_HEADER = 0x48445231u,      // "HDR1"
    RAW_VIDEO_FRAME = 0x52415746u,    // "RAWF"
    IMU_GYRO_ACCEL = 0x494D5553u,     // "IMUS"
    PCM_FLOAT_AUDIO = 0x41554446u,    // "AUDF"
    JOURNAL_INDEX_CKPT = 0x4A494458u, // "JIDX" (Periodic crash-recovery checkpoint)
    FINAL_FOOTER_INDEX = 0x46494458u  // "FIDX"
};

#pragma pack(push, 1)
struct AcrawFileHeader {
    uint8_t magic[8]{'A', 'C', 'R', 'A', 'W', 0x01, 0x00, 0x00};
    uint32_t width{0};
    uint32_t height{0};
    uint8_t cfaPattern{0};
    uint8_t bitDepth{12};
    uint16_t blackLevelRggb[4]{256, 256, 256, 256};
    uint16_t whiteLevel{4095};
    float targetFps{24.0f};
    float rollingShutterReadoutMs{11.2f};
    uint32_t audioSampleRateHz{48000};
    uint16_t audioChannels{2};
    uint16_t journalCheckpointIntervalFrames{16};
    float forwardMatrixD65[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
    float forwardMatrixStdA[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
    char cameraModel[64]{};
    char acesIdtName[64]{};
    char lookLutName[64]{};
    uint32_t headerCrc32c{0};
};

struct AcrawChunkHeader {
    uint32_t chunkType{0}; // AcrawChunkType
    uint32_t payloadBytes{0};
    uint64_t sequenceNumber{0};
    int64_t timestampNs{0};
    uint32_t payloadCrc32c{0};
    uint32_t headerCrc32c{0}; // CRC32C of the preceding 24 bytes of AcrawChunkHeader
};

struct AcrawFrameMetadataPrefix {
    uint64_t frameNumber{0};
    int64_t exposureTimeNs{0};
    int64_t rollingShutterNs{0};
    int32_t isoSensitivity{400};
    float focusDistanceDiopters{0.0f};
    float dynamicBlackLevelRggb[4]{256.0f, 256.0f, 256.0f, 256.0f};
    uint32_t whiteLevel{4095};
    float wbNeutralRggb[3]{0.48f, 1.0f, 0.62f};
};

struct AcrawFrameIndexEntry {
    uint64_t frameSequenceNumber{0};
    int64_t timestampNs{0};
    uint64_t fileOffsetBytes{0};
    uint32_t chunkPayloadBytes{0};
    uint32_t frameCrc32c{0};
};
#pragma pack(pop)

/**
 * Hardware/table-accelerated CRC32C (Castagnoli polynomial 0x1EDC6F41).
 */
uint32_t computeCrc32c(const void* data, size_t length, uint32_t initial = 0) noexcept;

/**
 * Layer 3: Clean-Room Open Container Writer (`.acraw` v1).
 *
 * - Preallocates file capacity upfront via posix_fallocate().
 * - Buffers chunks into 2 MiB aligned sequential write blocks flushed via pwritev()
 *   (zero per-frame fsync!).
 * - Computes CRC32C for every chunk header and every frame payload.
 * - Writes inline JOURNAL_INDEX_CKPT chunks every N frames so power loss or process kill
 *   is 100% crash-recoverable without losing a single written frame.
 */
class ApexRawContainerWriter {
public:
    ApexRawContainerWriter() = default;
    ~ApexRawContainerWriter() noexcept;

    bool openAndPreallocate(const std::string& filePath,
                            const AcrawFileHeader& header,
                            uint64_t preallocateBytes = 64ULL * 1024ULL * 1024ULL) noexcept;

    bool writeVideoFrame(uint64_t frameSequence,
                         int64_t timestampNs,
                         const AcrawFrameMetadataPrefix& meta,
                         const uint8_t* compressedBitstream,
                         size_t bitstreamBytes) noexcept;

    bool writeImuBatch(uint64_t batchSeq,
                       int64_t timestampNs,
                       const l1::ImuSample* samples,
                       size_t sampleCount) noexcept;

    bool writeAudioPcmFloat(uint64_t audioSeq,
                            int64_t timestampNs,
                            const float* interleavedStereoFloat,
                            size_t floatSampleCount) noexcept;

    bool finalizeAndClose() noexcept;

    /**
     * Test-only helper: simulates an abrupt process kill or power loss by closing the file
     * descriptor immediately after flushing buffered bytes (without writing FINAL_FOOTER_INDEX).
     */
    void abortSimulatePowerLossMidTake() noexcept;

    uint64_t bytesWritten() const noexcept { return currentFileOffset_; }
    size_t framesIndexed() const noexcept { return frameIndex_.size(); }

private:
    bool appendRawBytes(const void* data, size_t len) noexcept;
    bool flushWriteBufferToDisk() noexcept;
    bool writeJournalCheckpointChunk() noexcept;

    int fd_{-1};
    uint64_t currentFileOffset_{0};
    uint64_t diskFlushedOffset_{0};
    uint16_t checkpointInterval_{16};
    uint32_t checkpointsWritten_{0};
    size_t lastCheckpointedFrameIdx_{0};
    std::vector<uint8_t> writeStagingBuffer_{};
    size_t stagingUsed_{0};
    std::vector<AcrawFrameIndexEntry> frameIndex_{};
};

struct ContainerRecoveryReport {
    bool openedCleanly{false};
    bool wasRecoveredFromCrash{false};
    size_t journalCheckpointsFound{0};
    size_t totalValidFramesRecovered{0};
    size_t totalImuBatchesRecovered{0};
    size_t totalAudioChunksRecovered{0};
    size_t corruptedTailBytesTruncated{0};
    uint64_t lastValidChunkEndOffset{0};
    AcrawFileHeader fileHeader{};
    std::vector<AcrawFrameIndexEntry> frameIndex{};
};

/**
 * Layer 3: Clean-Room Open Container Reader & Crash-Recovery Engine (All fields Little-Endian).
 */
class ApexRawContainerReader {
public:
    /**
     * Opens an .acraw file, verifies header CRC32C, and if the final footer index is missing
     * or truncated (due to crash/power loss), scans forward verifying chunk CRC32C checksums
     * to reconstruct 100% of completed frames.
     */
    static ContainerRecoveryReport openAndRecover(const std::string& filePath);

    /**
     * Repairs a crash-truncated or torn-tail .acraw file in-place by truncating to the last
     * CRC-verified chunk boundary and appending a clean FINAL_FOOTER_INDEX + fdatasync().
     */
    static bool repairAndFinalizeTruncatedFile(const std::string& filePath);

    static bool readAndVerifyVideoFrame(const std::string& filePath,
                                        const AcrawFrameIndexEntry& entry,
                                        AcrawFrameMetadataPrefix& outMeta,
                                        std::vector<uint8_t>& outCompressedBitstream);
};

} // namespace apexcine::l3
