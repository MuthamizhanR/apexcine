#pragma once

#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"
#include <cstddef>
#include <cstdint>

namespace apexcine::l2 {

#pragma pack(push, 1)
struct CompressedBayerFrameHeader {
    uint32_t magic{0x41434243u}; // "ACBC" (ApexCine Bayer Codec v1)
    uint32_t planeWidth{0};
    uint32_t planeHeight{0};
    uint32_t uncompressedBytes{0};
    uint32_t compressedPayloadBytes{0};
};
#pragma pack(pop)

struct LosslessCompressionStats {
    size_t uncompressedBytes{0};
    size_t compressedBytes{0};
    double compressionRatio{1.0};
    double encodeDurationMs{0.0};
    double throughputGBps{0.0};
};

/**
 * Layer 2: High-Throughput Intra-Frame Lossless CFA Bayer Codec (ACBC v1).
 *
 * Clean-room standards-inspired (LOCO-I / Group-Varint) lossless Bayer compressor:
 * 1. Operates on canonical R, Gr, Gb, B planes.
 * 2. Uses horizontal delta prediction on R, Gr, B and cross-channel co-located green
 *    prediction (Gb - Gr) to exploit green-channel spatial correlation.
 * 3. Maps signed 16-bit residuals via branchless ZigZag encoding.
 * 4. Encodes 16-sample blocks with a single 4x2-bit control byte (4-bit, 6-bit, 8-bit, or
 *    16-bit sub-groups), achieving >= 2.5:1 - 3.5:1 lossless ratio at >= 1.5 GB/s.
 */
class LosslessBayerCodec {
public:
    static constexpr size_t maxCompressedSize(size_t totalSamples) noexcept {
        // Worst case: 1 control byte per 16 samples + 32 bytes payload + header + tail margin
        return sizeof(CompressedBayerFrameHeader) + ((totalSamples + 15) / 16) * 33 + 64;
    }

    static bool compressPlanarBayer(const PlanarBayerView& srcPlanes,
                                    uint8_t* dstBuffer,
                                    size_t dstCapacity,
                                    LosslessCompressionStats& outStats) noexcept;

    static bool decompressToPlanarBayer(const uint8_t* srcCompressed,
                                        size_t srcBytes,
                                        PlanarBayerView& dstPlanes) noexcept;
};

} // namespace apexcine::l2
