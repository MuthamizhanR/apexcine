#include "apexcine/l2_pipeline/LosslessBayerCodec.hpp"

#include <chrono>
#include <cstring>

namespace apexcine::l2 {

namespace {
inline constexpr uint16_t zigZagEncode16(int16_t v) noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(v) << 1) ^ static_cast<uint16_t>(v >> 15));
}

inline constexpr int16_t zigZagDecode16(uint16_t u) noexcept {
    return static_cast<int16_t>((u >> 1) ^ static_cast<uint16_t>(-static_cast<int16_t>(u & 1u)));
}

inline uint8_t encodeSubgroup4(const uint16_t zz[4], uint8_t*& outPtr) noexcept {
    const uint16_t maxOr = zz[0] | zz[1] | zz[2] | zz[3];
    if (maxOr <= 0x000Fu) {
        // Tag 0: 4 bits per sample -> 2 bytes total
        const uint16_t packed16 = static_cast<uint16_t>(
            zz[0] | (zz[1] << 4) | (zz[2] << 8) | (zz[3] << 12));
        std::memcpy(outPtr, &packed16, sizeof(uint16_t));
        outPtr += 2;
        return 0u;
    }
    if (maxOr <= 0x003Fu) {
        // Tag 1: 6 bits per sample -> 3 bytes total
        const uint32_t packed24 = static_cast<uint32_t>(zz[0]) |
                                  (static_cast<uint32_t>(zz[1]) << 6) |
                                  (static_cast<uint32_t>(zz[2]) << 12) |
                                  (static_cast<uint32_t>(zz[3]) << 18);
        outPtr[0] = static_cast<uint8_t>(packed24 & 0xFFu);
        outPtr[1] = static_cast<uint8_t>((packed24 >> 8) & 0xFFu);
        outPtr[2] = static_cast<uint8_t>((packed24 >> 16) & 0xFFu);
        outPtr += 3;
        return 1u;
    }
    if (maxOr <= 0x00FFu) {
        // Tag 2: 8 bits per sample -> 4 bytes total
        outPtr[0] = static_cast<uint8_t>(zz[0]);
        outPtr[1] = static_cast<uint8_t>(zz[1]);
        outPtr[2] = static_cast<uint8_t>(zz[2]);
        outPtr[3] = static_cast<uint8_t>(zz[3]);
        outPtr += 4;
        return 2u;
    }
    // Tag 3: full 16 bits per sample -> 8 bytes total
    std::memcpy(outPtr, zz, 4 * sizeof(uint16_t));
    outPtr += 8;
    return 3u;
}

inline bool decodeSubgroup4(uint8_t tag,
                            const uint8_t*& inPtr,
                            const uint8_t* inEnd,
                            uint16_t outZz[4]) noexcept {
    if (tag == 0u) {
        if (inPtr + 2 > inEnd) return false;
        uint16_t packed16 = 0;
        std::memcpy(&packed16, inPtr, sizeof(uint16_t));
        inPtr += 2;
        outZz[0] = packed16 & 0x0Fu;
        outZz[1] = (packed16 >> 4) & 0x0Fu;
        outZz[2] = (packed16 >> 8) & 0x0Fu;
        outZz[3] = (packed16 >> 12) & 0x0Fu;
        return true;
    }
    if (tag == 1u) {
        if (inPtr + 3 > inEnd) return false;
        const uint32_t packed24 = static_cast<uint32_t>(inPtr[0]) |
                                  (static_cast<uint32_t>(inPtr[1]) << 8) |
                                  (static_cast<uint32_t>(inPtr[2]) << 16);
        inPtr += 3;
        outZz[0] = static_cast<uint16_t>(packed24 & 0x3Fu);
        outZz[1] = static_cast<uint16_t>((packed24 >> 6) & 0x3Fu);
        outZz[2] = static_cast<uint16_t>((packed24 >> 12) & 0x3Fu);
        outZz[3] = static_cast<uint16_t>((packed24 >> 18) & 0x3Fu);
        return true;
    }
    if (tag == 2u) {
        if (inPtr + 4 > inEnd) return false;
        outZz[0] = inPtr[0];
        outZz[1] = inPtr[1];
        outZz[2] = inPtr[2];
        outZz[3] = inPtr[3];
        inPtr += 4;
        return true;
    }
    if (inPtr + 8 > inEnd) return false;
    std::memcpy(outZz, inPtr, 4 * sizeof(uint16_t));
    inPtr += 8;
    return true;
}

void compressPlane1D(const uint16_t* plane,
                     const uint16_t* refGreenPlane,
                     size_t numSamples,
                     uint8_t*& outPtr) noexcept {
    uint16_t prev = 0;
    size_t i = 0;
    uint16_t zzBlock[16];

    for (; i + 16 <= numSamples; i += 16) {
        if (refGreenPlane != nullptr) {
            for (size_t k = 0; k < 16; ++k) {
                const int16_t diff = static_cast<int16_t>(plane[i + k] - refGreenPlane[i + k]);
                zzBlock[k] = zigZagEncode16(diff);
            }
        } else {
            for (size_t k = 0; k < 16; ++k) {
                const uint16_t cur = plane[i + k];
                const int16_t diff = static_cast<int16_t>(cur - prev);
                zzBlock[k] = zigZagEncode16(diff);
                prev = cur;
            }
        }

        uint8_t* ctrlPtr = outPtr++;
        const uint8_t t0 = encodeSubgroup4(&zzBlock[0], outPtr);
        const uint8_t t1 = encodeSubgroup4(&zzBlock[4], outPtr);
        const uint8_t t2 = encodeSubgroup4(&zzBlock[8], outPtr);
        const uint8_t t3 = encodeSubgroup4(&zzBlock[12], outPtr);
        *ctrlPtr = static_cast<uint8_t>(t0 | (t1 << 2) | (t2 << 4) | (t3 << 6));
    }

    // Remaining tail samples (< 16)
    for (; i < numSamples; ++i) {
        const uint16_t cur = plane[i];
        std::memcpy(outPtr, &cur, sizeof(uint16_t));
        outPtr += 2;
    }
}

bool decompressPlane1D(const uint8_t*& inPtr,
                       const uint8_t* inEnd,
                       const uint16_t* refGreenPlane,
                       size_t numSamples,
                       uint16_t* dstPlane) noexcept {
    uint16_t prev = 0;
    size_t i = 0;
    uint16_t zzBlock[16];

    for (; i + 16 <= numSamples; i += 16) {
        if (inPtr >= inEnd) return false;
        const uint8_t ctrl = *inPtr++;
        if (!decodeSubgroup4((ctrl >> 0) & 0x03u, inPtr, inEnd, &zzBlock[0]) ||
            !decodeSubgroup4((ctrl >> 2) & 0x03u, inPtr, inEnd, &zzBlock[4]) ||
            !decodeSubgroup4((ctrl >> 4) & 0x03u, inPtr, inEnd, &zzBlock[8]) ||
            !decodeSubgroup4((ctrl >> 6) & 0x03u, inPtr, inEnd, &zzBlock[12])) {
            return false;
        }

        if (refGreenPlane != nullptr) {
            for (size_t k = 0; k < 16; ++k) {
                const int16_t diff = zigZagDecode16(zzBlock[k]);
                dstPlane[i + k] = static_cast<uint16_t>(refGreenPlane[i + k] + diff);
            }
        } else {
            for (size_t k = 0; k < 16; ++k) {
                const int16_t diff = zigZagDecode16(zzBlock[k]);
                prev = static_cast<uint16_t>(prev + diff);
                dstPlane[i + k] = prev;
            }
        }
    }

    for (; i < numSamples; ++i) {
        if (inPtr + 2 > inEnd) return false;
        std::memcpy(&dstPlane[i], inPtr, sizeof(uint16_t));
        inPtr += 2;
    }
    return true;
}
} // namespace

bool LosslessBayerCodec::compressPlanarBayer(const PlanarBayerView& srcPlanes,
                                             uint8_t* dstBuffer,
                                             size_t dstCapacity,
                                             LosslessCompressionStats& outStats) noexcept {
    const size_t planeSamples = srcPlanes.samplesPerPlane();
    const size_t totalSamples = planeSamples * 4;
    if (planeSamples == 0 || srcPlanes.planeR == nullptr || dstBuffer == nullptr ||
        dstCapacity < maxCompressedSize(totalSamples)) {
        return false;
    }

    const auto t0 = std::chrono::steady_clock::now();

    uint8_t* writePtr = dstBuffer + sizeof(CompressedBayerFrameHeader);
    compressPlane1D(srcPlanes.planeR, nullptr, planeSamples, writePtr);
    compressPlane1D(srcPlanes.planeGr, nullptr, planeSamples, writePtr);
    // Predict Gb from co-located Gr for maximum lossless compression ratio
    compressPlane1D(srcPlanes.planeGb, srcPlanes.planeGr, planeSamples, writePtr);
    compressPlane1D(srcPlanes.planeB, nullptr, planeSamples, writePtr);

    const size_t totalCompressed = static_cast<size_t>(writePtr - dstBuffer);
    CompressedBayerFrameHeader hdr{};
    hdr.magic = 0x41434243u;
    hdr.planeWidth = srcPlanes.planeWidth;
    hdr.planeHeight = srcPlanes.planeHeight;
    hdr.uncompressedBytes = static_cast<uint32_t>(totalSamples * sizeof(uint16_t));
    hdr.compressedPayloadBytes =
        static_cast<uint32_t>(totalCompressed - sizeof(CompressedBayerFrameHeader));
    std::memcpy(dstBuffer, &hdr, sizeof(hdr));

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedNs =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());

    outStats.uncompressedBytes = hdr.uncompressedBytes;
    outStats.compressedBytes = totalCompressed;
    outStats.compressionRatio =
        static_cast<double>(hdr.uncompressedBytes) / static_cast<double>(totalCompressed);
    outStats.encodeDurationMs = elapsedNs / 1e6;
    outStats.throughputGBps =
        (elapsedNs > 0.0) ? (static_cast<double>(hdr.uncompressedBytes) / elapsedNs) : 0.0;
    return true;
}

bool LosslessBayerCodec::decompressToPlanarBayer(const uint8_t* srcCompressed,
                                                 size_t srcBytes,
                                                 PlanarBayerView& dstPlanes) noexcept {
    if (srcCompressed == nullptr || srcBytes < sizeof(CompressedBayerFrameHeader) ||
        dstPlanes.planeR == nullptr) {
        return false;
    }

    const size_t planeSamples = dstPlanes.samplesPerPlane();
    const size_t expectedUncompressedBytes = planeSamples * 4 * sizeof(uint16_t);
    CompressedBayerFrameHeader hdr{};
    std::memcpy(&hdr, srcCompressed, sizeof(hdr));
    if (hdr.magic != 0x41434243u || hdr.planeWidth != dstPlanes.planeWidth ||
        hdr.planeHeight != dstPlanes.planeHeight ||
        hdr.uncompressedBytes != expectedUncompressedBytes ||
        sizeof(hdr) + hdr.compressedPayloadBytes > srcBytes) {
        return false;
    }
    const uint8_t* readPtr = srcCompressed + sizeof(hdr);
    const uint8_t* endPtr = readPtr + hdr.compressedPayloadBytes;

    if (!decompressPlane1D(readPtr, endPtr, nullptr, planeSamples, dstPlanes.planeR)) return false;
    if (!decompressPlane1D(readPtr, endPtr, nullptr, planeSamples, dstPlanes.planeGr)) return false;
    if (!decompressPlane1D(readPtr, endPtr, dstPlanes.planeGr, planeSamples, dstPlanes.planeGb)) {
        return false;
    }
    if (!decompressPlane1D(readPtr, endPtr, nullptr, planeSamples, dstPlanes.planeB)) return false;

    return readPtr == endPtr;
}

} // namespace apexcine::l2
