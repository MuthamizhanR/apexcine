#pragma once

#include "apexcine/l0_runtime/BufferContract.hpp"
#include <cstddef>
#include <cstdint>

namespace apexcine::l2 {

/**
 * 4-plane separated Bayer representation in a single contiguous buffer.
 * Each plane has dimensions (width / 2) x (height / 2) of uint16_t.
 * Canonical plane order is always normalized to:
 *   Plane 0: R
 *   Plane 1: Gr (Green on Red row)
 *   Plane 2: Gb (Green on Blue row)
 *   Plane 3: B
 */
struct PlanarBayerView {
    uint16_t* planeR{nullptr};
    uint16_t* planeGr{nullptr};
    uint16_t* planeGb{nullptr};
    uint16_t* planeB{nullptr};
    uint32_t planeWidth{0};  // fullWidth / 2
    uint32_t planeHeight{0}; // fullHeight / 2

    constexpr size_t samplesPerPlane() const noexcept {
        return static_cast<size_t>(planeWidth) * static_cast<size_t>(planeHeight);
    }

    constexpr size_t totalBytes() const noexcept {
        return samplesPerPlane() * 4 * sizeof(uint16_t);
    }
};

/**
 * Layer 2: High-Speed MIPI RAW10 / RAW12 / RAW16 Unpacker & CFA Planarizer.
 *
 * - Strictly honors rowStrideBytes (handles arbitrary OEM row padding).
 * - Unpacks packed MIPI CSI-2 RAW10 (5B -> 4px) and RAW12 (3B -> 2px) or RAW16
 *   directly into 4 canonical R/Gr/Gb/B planes in a single pass.
 * - Uses ARM NEON intrinsics when compiled for arm64-v8a and unrolled vector-friendly
 *   loops on x86_64.
 */
class RawUnpackAndPlanarize {
public:
    static PlanarBayerView bindPlanarBuffer(uint16_t* contiguousBase,
                                            uint32_t fullWidth,
                                            uint32_t fullHeight) noexcept;

    static bool unpackToCanonicalPlanes(const uint8_t* srcPacked,
                                        uint32_t fullWidth,
                                        uint32_t fullHeight,
                                        uint32_t rowStrideBytes,
                                        l0::PixelFormat format,
                                        l0::CfaPattern cfaPattern,
                                        PlanarBayerView& dstPlanes) noexcept;

    static bool reconstructInterleavedRaw16(const PlanarBayerView& srcPlanes,
                                            l0::CfaPattern targetCfa,
                                            uint16_t* dstInterleaved,
                                            uint32_t dstRowStrideShorts) noexcept;

    /**
     * Packs interleaved 16-bit Bayer into MIPI CSI-2 RAW10 or RAW12 (for test synthesis & verification).
     */
    static void packInterleavedToMipiRaw10(const uint16_t* srcRaw16,
                                           uint32_t fullWidth,
                                           uint32_t fullHeight,
                                           uint32_t dstRowStrideBytes,
                                           uint8_t* dstRaw10) noexcept;

    static void packInterleavedToMipiRaw12(const uint16_t* srcRaw16,
                                           uint32_t fullWidth,
                                           uint32_t fullHeight,
                                           uint32_t dstRowStrideBytes,
                                           uint8_t* dstRaw12) noexcept;
};

} // namespace apexcine::l2
