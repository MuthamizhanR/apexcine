#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"

#include <cstring>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace apexcine::l2 {

namespace {
inline void mapCfaTargets(l0::CfaPattern cfa,
                          const PlanarBayerView& dst,
                          uint16_t*& evenRowEvenCol,
                          uint16_t*& evenRowOddCol,
                          uint16_t*& oddRowEvenCol,
                          uint16_t*& oddRowOddCol) noexcept {
    switch (cfa) {
        case l0::CfaPattern::RGGB:
            evenRowEvenCol = dst.planeR;
            evenRowOddCol = dst.planeGr;
            oddRowEvenCol = dst.planeGb;
            oddRowOddCol = dst.planeB;
            break;
        case l0::CfaPattern::GRBG:
            evenRowEvenCol = dst.planeGr;
            evenRowOddCol = dst.planeR;
            oddRowEvenCol = dst.planeB;
            oddRowOddCol = dst.planeGb;
            break;
        case l0::CfaPattern::GBRG:
            evenRowEvenCol = dst.planeGb;
            evenRowOddCol = dst.planeB;
            oddRowEvenCol = dst.planeR;
            oddRowOddCol = dst.planeGr;
            break;
        case l0::CfaPattern::BGGR:
            evenRowEvenCol = dst.planeB;
            evenRowOddCol = dst.planeGb;
            oddRowEvenCol = dst.planeGr;
            oddRowOddCol = dst.planeR;
            break;
    }
}
} // namespace

PlanarBayerView RawUnpackAndPlanarize::bindPlanarBuffer(uint16_t* contiguousBase,
                                                        uint32_t fullWidth,
                                                        uint32_t fullHeight) noexcept {
    PlanarBayerView view{};
    view.planeWidth = fullWidth / 2;
    view.planeHeight = fullHeight / 2;
    const size_t planeSamples = view.samplesPerPlane();
    view.planeR = contiguousBase;
    view.planeGr = contiguousBase + planeSamples;
    view.planeGb = contiguousBase + planeSamples * 2;
    view.planeB = contiguousBase + planeSamples * 3;
    return view;
}

bool RawUnpackAndPlanarize::unpackToCanonicalPlanes(const uint8_t* srcPacked,
                                                    uint32_t fullWidth,
                                                    uint32_t fullHeight,
                                                    uint32_t rowStrideBytes,
                                                    l0::PixelFormat format,
                                                    l0::CfaPattern cfaPattern,
                                                    PlanarBayerView& dstPlanes) noexcept {
    if (srcPacked == nullptr || fullWidth < 2 || fullHeight < 2 ||
        (fullWidth & 1u) != 0 || (fullHeight & 1u) != 0 ||
        dstPlanes.planeR == nullptr) {
        return false;
    }

    uint16_t* p00 = nullptr;
    uint16_t* p01 = nullptr;
    uint16_t* p10 = nullptr;
    uint16_t* p11 = nullptr;
    mapCfaTargets(cfaPattern, dstPlanes, p00, p01, p10, p11);

    const uint32_t halfW = fullWidth / 2;
    const uint32_t halfH = fullHeight / 2;

    if (format == l0::PixelFormat::RAW16_SENSOR_LE) {
        if (rowStrideBytes < fullWidth * sizeof(uint16_t)) return false;
        for (uint32_t py = 0; py < halfH; ++py) {
            const auto* rowEven = reinterpret_cast<const uint16_t*>(
                srcPacked + static_cast<size_t>(2 * py) * rowStrideBytes);
            const auto* rowOdd = reinterpret_cast<const uint16_t*>(
                srcPacked + static_cast<size_t>(2 * py + 1) * rowStrideBytes);
            uint16_t* out00 = p00 + static_cast<size_t>(py) * halfW;
            uint16_t* out01 = p01 + static_cast<size_t>(py) * halfW;
            uint16_t* out10 = p10 + static_cast<size_t>(py) * halfW;
            uint16_t* out11 = p11 + static_cast<size_t>(py) * halfW;

            uint32_t px = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
            for (; px + 8 <= halfW; px += 8) {
                uint16x8x2_t evenDeint = vld2q_u16(rowEven + 2 * px);
                uint16x8x2_t oddDeint = vld2q_u16(rowOdd + 2 * px);
                vst1q_u16(out00 + px, evenDeint.val[0]);
                vst1q_u16(out01 + px, evenDeint.val[1]);
                vst1q_u16(out10 + px, oddDeint.val[0]);
                vst1q_u16(out11 + px, oddDeint.val[1]);
            }
#endif
            for (; px < halfW; ++px) {
                out00[px] = rowEven[2 * px];
                out01[px] = rowEven[2 * px + 1];
                out10[px] = rowOdd[2 * px];
                out11[px] = rowOdd[2 * px + 1];
            }
        }
        return true;
    }

    if (format == l0::PixelFormat::RAW12_MIPI_PACKED) {
        if (rowStrideBytes < (fullWidth * 3) / 2) return false;
        for (uint32_t py = 0; py < halfH; ++py) {
            const uint8_t* rowEven = srcPacked + static_cast<size_t>(2 * py) * rowStrideBytes;
            const uint8_t* rowOdd = srcPacked + static_cast<size_t>(2 * py + 1) * rowStrideBytes;
            uint16_t* out00 = p00 + static_cast<size_t>(py) * halfW;
            uint16_t* out01 = p01 + static_cast<size_t>(py) * halfW;
            uint16_t* out10 = p10 + static_cast<size_t>(py) * halfW;
            uint16_t* out11 = p11 + static_cast<size_t>(py) * halfW;

            for (uint32_t px = 0; px < halfW; ++px) {
                const size_t byteOffset = static_cast<size_t>(px) * 3;
                // MIPI RAW12: [P0_11:4][P1_11:4][P1_3:0 | P0_3:0]
                const uint8_t e0 = rowEven[byteOffset];
                const uint8_t e1 = rowEven[byteOffset + 1];
                const uint8_t e2 = rowEven[byteOffset + 2];
                out00[px] = static_cast<uint16_t>((static_cast<uint16_t>(e0) << 4) | (e2 & 0x0Fu));
                out01[px] = static_cast<uint16_t>((static_cast<uint16_t>(e1) << 4) | (e2 >> 4));

                const uint8_t o0 = rowOdd[byteOffset];
                const uint8_t o1 = rowOdd[byteOffset + 1];
                const uint8_t o2 = rowOdd[byteOffset + 2];
                out10[px] = static_cast<uint16_t>((static_cast<uint16_t>(o0) << 4) | (o2 & 0x0Fu));
                out11[px] = static_cast<uint16_t>((static_cast<uint16_t>(o1) << 4) | (o2 >> 4));
            }
        }
        return true;
    }

    if (format == l0::PixelFormat::RAW10_MIPI_PACKED) {
        if ((fullWidth % 4) != 0 || rowStrideBytes < (fullWidth * 5) / 4) return false;
        for (uint32_t py = 0; py < halfH; ++py) {
            const uint8_t* rowEven = srcPacked + static_cast<size_t>(2 * py) * rowStrideBytes;
            const uint8_t* rowOdd = srcPacked + static_cast<size_t>(2 * py + 1) * rowStrideBytes;
            uint16_t* out00 = p00 + static_cast<size_t>(py) * halfW;
            uint16_t* out01 = p01 + static_cast<size_t>(py) * halfW;
            uint16_t* out10 = p10 + static_cast<size_t>(py) * halfW;
            uint16_t* out11 = p11 + static_cast<size_t>(py) * halfW;

            // Process 4 horizontal pixels (2 CFA pairs = 5 bytes) per iteration
            for (uint32_t px = 0; px < halfW; px += 2) {
                const size_t bOff = (static_cast<size_t>(px) / 2) * 5;
                const uint8_t e0 = rowEven[bOff + 0];
                const uint8_t e1 = rowEven[bOff + 1];
                const uint8_t e2 = rowEven[bOff + 2];
                const uint8_t e3 = rowEven[bOff + 3];
                const uint8_t e4 = rowEven[bOff + 4];

                out00[px + 0] = static_cast<uint16_t>((static_cast<uint16_t>(e0) << 2) | ((e4 >> 0) & 0x03u));
                out01[px + 0] = static_cast<uint16_t>((static_cast<uint16_t>(e1) << 2) | ((e4 >> 2) & 0x03u));
                out00[px + 1] = static_cast<uint16_t>((static_cast<uint16_t>(e2) << 2) | ((e4 >> 4) & 0x03u));
                out01[px + 1] = static_cast<uint16_t>((static_cast<uint16_t>(e3) << 2) | ((e4 >> 6) & 0x03u));

                const uint8_t o0 = rowOdd[bOff + 0];
                const uint8_t o1 = rowOdd[bOff + 1];
                const uint8_t o2 = rowOdd[bOff + 2];
                const uint8_t o3 = rowOdd[bOff + 3];
                const uint8_t o4 = rowOdd[bOff + 4];

                out10[px + 0] = static_cast<uint16_t>((static_cast<uint16_t>(o0) << 2) | ((o4 >> 0) & 0x03u));
                out11[px + 0] = static_cast<uint16_t>((static_cast<uint16_t>(o1) << 2) | ((o4 >> 2) & 0x03u));
                out10[px + 1] = static_cast<uint16_t>((static_cast<uint16_t>(o2) << 2) | ((o4 >> 4) & 0x03u));
                out11[px + 1] = static_cast<uint16_t>((static_cast<uint16_t>(o3) << 2) | ((o4 >> 6) & 0x03u));
            }
        }
        return true;
    }

    return false;
}

bool RawUnpackAndPlanarize::reconstructInterleavedRaw16(const PlanarBayerView& srcPlanes,
                                                        l0::CfaPattern targetCfa,
                                                        uint16_t* dstInterleaved,
                                                        uint32_t dstRowStrideShorts) noexcept {
    if (srcPlanes.planeR == nullptr || dstInterleaved == nullptr ||
        dstRowStrideShorts < srcPlanes.planeWidth * 2) {
        return false;
    }

    uint16_t* p00 = nullptr;
    uint16_t* p01 = nullptr;
    uint16_t* p10 = nullptr;
    uint16_t* p11 = nullptr;
    mapCfaTargets(targetCfa, srcPlanes, p00, p01, p10, p11);

    const uint32_t halfW = srcPlanes.planeWidth;
    const uint32_t halfH = srcPlanes.planeHeight;

    for (uint32_t py = 0; py < halfH; ++py) {
        uint16_t* rowEven = dstInterleaved + static_cast<size_t>(2 * py) * dstRowStrideShorts;
        uint16_t* rowOdd = dstInterleaved + static_cast<size_t>(2 * py + 1) * dstRowStrideShorts;
        const uint16_t* in00 = p00 + static_cast<size_t>(py) * halfW;
        const uint16_t* in01 = p01 + static_cast<size_t>(py) * halfW;
        const uint16_t* in10 = p10 + static_cast<size_t>(py) * halfW;
        const uint16_t* in11 = p11 + static_cast<size_t>(py) * halfW;

        for (uint32_t px = 0; px < halfW; ++px) {
            rowEven[2 * px] = in00[px];
            rowEven[2 * px + 1] = in01[px];
            rowOdd[2 * px] = in10[px];
            rowOdd[2 * px + 1] = in11[px];
        }
    }
    return true;
}

void RawUnpackAndPlanarize::packInterleavedToMipiRaw10(const uint16_t* srcRaw16,
                                                       uint32_t fullWidth,
                                                       uint32_t fullHeight,
                                                       uint32_t dstRowStrideBytes,
                                                       uint8_t* dstRaw10) noexcept {
    for (uint32_t y = 0; y < fullHeight; ++y) {
        const uint16_t* srcRow = srcRaw16 + static_cast<size_t>(y) * fullWidth;
        uint8_t* dstRow = dstRaw10 + static_cast<size_t>(y) * dstRowStrideBytes;
        for (uint32_t x = 0; x + 3 < fullWidth; x += 4) {
            const uint16_t p0 = srcRow[x + 0] & 0x03FFu;
            const uint16_t p1 = srcRow[x + 1] & 0x03FFu;
            const uint16_t p2 = srcRow[x + 2] & 0x03FFu;
            const uint16_t p3 = srcRow[x + 3] & 0x03FFu;
            const size_t bOff = (static_cast<size_t>(x) / 4) * 5;
            dstRow[bOff + 0] = static_cast<uint8_t>(p0 >> 2);
            dstRow[bOff + 1] = static_cast<uint8_t>(p1 >> 2);
            dstRow[bOff + 2] = static_cast<uint8_t>(p2 >> 2);
            dstRow[bOff + 3] = static_cast<uint8_t>(p3 >> 2);
            dstRow[bOff + 4] = static_cast<uint8_t>(
                ((p0 & 0x03u) << 0) | ((p1 & 0x03u) << 2) |
                ((p2 & 0x03u) << 4) | ((p3 & 0x03u) << 6));
        }
    }
}

void RawUnpackAndPlanarize::packInterleavedToMipiRaw12(const uint16_t* srcRaw16,
                                                       uint32_t fullWidth,
                                                       uint32_t fullHeight,
                                                       uint32_t dstRowStrideBytes,
                                                       uint8_t* dstRaw12) noexcept {
    for (uint32_t y = 0; y < fullHeight; ++y) {
        const uint16_t* srcRow = srcRaw16 + static_cast<size_t>(y) * fullWidth;
        uint8_t* dstRow = dstRaw12 + static_cast<size_t>(y) * dstRowStrideBytes;
        for (uint32_t x = 0; x + 1 < fullWidth; x += 2) {
            const uint16_t p0 = srcRow[x + 0] & 0x0FFFu;
            const uint16_t p1 = srcRow[x + 1] & 0x0FFFu;
            const size_t bOff = (static_cast<size_t>(x) / 2) * 3;
            dstRow[bOff + 0] = static_cast<uint8_t>(p0 >> 4);
            dstRow[bOff + 1] = static_cast<uint8_t>(p1 >> 4);
            dstRow[bOff + 2] = static_cast<uint8_t>((p0 & 0x0Fu) | ((p1 & 0x0Fu) << 4));
        }
    }
}

} // namespace apexcine::l2
