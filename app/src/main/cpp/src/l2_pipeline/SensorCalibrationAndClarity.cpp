#include "apexcine/l2_pipeline/SensorCalibrationAndClarity.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace apexcine::l2 {

namespace {
inline uint16_t medianOf4(uint16_t a, uint16_t b, uint16_t c, uint16_t d) noexcept {
    if (a > b) std::swap(a, b);
    if (c > d) std::swap(c, d);
    if (a > c) std::swap(a, c);
    if (b > d) std::swap(b, d);
    return static_cast<uint16_t>((static_cast<uint32_t>(b) + static_cast<uint32_t>(c)) >> 1);
}

uint32_t repairPlaneImpulsesAndBanding(uint16_t* plane,
                                       uint32_t w,
                                       uint32_t h,
                                       uint16_t blackLevel,
                                       uint16_t whiteLevel,
                                       bool repairRowBanding,
                                       float& outBandingRms) noexcept {
    if (plane == nullptr || w < 4 || h < 4) return 0;

    uint32_t repaired = 0;
    const int32_t impulseThreshold = std::max<int32_t>(64, (whiteLevel - blackLevel) / 5);

    // 1. Bad-pixel & PDAF isolated impulse repair within same-color CFA plane
    for (uint32_t y = 1; y + 1 < h; ++y) {
        uint16_t* row = plane + static_cast<size_t>(y) * w;
        const uint16_t* rowUp = plane + static_cast<size_t>(y - 1) * w;
        const uint16_t* rowDn = plane + static_cast<size_t>(y + 1) * w;

        for (uint32_t x = 1; x + 1 < w; ++x) {
            const uint16_t center = row[x];
            const uint16_t left = row[x - 1];
            const uint16_t right = row[x + 1];
            const uint16_t up = rowUp[x];
            const uint16_t down = rowDn[x];

            const uint16_t maxNeighbor = std::max({left, right, up, down});
            const uint16_t minNeighbor = std::min({left, right, up, down});

            if (static_cast<int32_t>(center) - static_cast<int32_t>(maxNeighbor) > impulseThreshold ||
                static_cast<int32_t>(minNeighbor) - static_cast<int32_t>(center) > impulseThreshold) {
                row[x] = medianOf4(left, right, up, down);
                ++repaired;
            }
        }
    }

    // 2. Row-wise Fixed-Pattern Noise (FPN) horizontal banding suppression
    if (repairRowBanding && h >= 8) {
        std::vector<double> rowMeans(h, 0.0);
        for (uint32_t y = 0; y < h; ++y) {
            const uint16_t* row = plane + static_cast<size_t>(y) * w;
            uint64_t sum = 0;
            for (uint32_t x = 0; x < w; ++x) {
                sum += row[x];
            }
            rowMeans[y] = static_cast<double>(sum) / static_cast<double>(w);
        }

        double rmsAccum = 0.0;
        for (uint32_t y = 0; y < h; ++y) {
            // Smooth local vertical neighborhood (5-row window) to preserve real scene gradients
            double localVerticalMean = 0.0;
            int count = 0;
            for (int dy = -2; dy <= 2; ++dy) {
                const int ny = std::clamp(static_cast<int>(y) + dy, 0, static_cast<int>(h) - 1);
                localVerticalMean += rowMeans[static_cast<size_t>(ny)];
                ++count;
            }
            localVerticalMean /= static_cast<double>(count);

            const double rowBias = rowMeans[y] - localVerticalMean;
            // Only suppress high-frequency row-to-row FPN offset within +/- 32 ADU
            if (std::abs(rowBias) > 0.5 && std::abs(rowBias) < 48.0) {
                const int32_t delta = static_cast<int32_t>(std::lround(rowBias));
                rmsAccum += rowBias * rowBias;
                uint16_t* row = plane + static_cast<size_t>(y) * w;
                for (uint32_t x = 0; x < w; ++x) {
                    const int32_t corrected =
                        std::clamp<int32_t>(static_cast<int32_t>(row[x]) - delta, 0, whiteLevel);
                    row[x] = static_cast<uint16_t>(corrected);
                }
            }
        }
        outBandingRms = static_cast<float>(std::sqrt(rmsAccum / static_cast<double>(h)));
    }

    return repaired;
}
} // namespace

EttrExposureTelemetry SensorCalibrationAndClarity::computeEttrStops(
    const PlanarBayerView& planes,
    const uint16_t dynamicBlackLevelRggb[4],
    uint16_t whiteLevel,
    uint32_t currentIso,
    const lp::DeviceProfileRecord& deviceProfile) noexcept {
    EttrExposureTelemetry ettr{};
    ettr.isDualConversionGainHigh = (currentIso >= deviceProfile.dcgJumpIso);
    ettr.recommendedDcgIso = deviceProfile.dcgJumpIso;
    ettr.estimatedDynamicRangeStops =
        ettr.isDualConversionGainHigh
            ? deviceProfile.maxDynamicRangeStops
            : (deviceProfile.maxDynamicRangeStops - 0.6f);

    const size_t n = planes.samplesPerPlane();
    if (n == 0 || planes.planeR == nullptr) {
        return ettr;
    }

    // 256-bin histogram of linear signal above black level across all 4 planes
    std::array<uint32_t, 256> hist{};
    const uint16_t* ptrs[4] = {planes.planeR, planes.planeGr, planes.planeGb, planes.planeB};
    const uint16_t clipCeiling =
        static_cast<uint16_t>(whiteLevel - std::max<uint16_t>(4, whiteLevel / 200));

    uint64_t clippedCount = 0;
    uint16_t peakCode = 0;
    double linearNormSum = 0.0;
    const size_t totalSamples = n * 4;

    for (int p = 0; p < 4; ++p) {
        const uint16_t* data = ptrs[p];
        const int32_t bl = dynamicBlackLevelRggb[p];
        const double range = std::max<double>(1.0, static_cast<double>(whiteLevel) - bl);

        for (size_t i = 0; i < n; ++i) {
            const uint16_t code = data[i];
            if (code > peakCode) peakCode = code;
            if (code >= clipCeiling) ++clippedCount;

            const double norm =
                std::clamp((static_cast<double>(code) - bl) / range, 0.0, 1.0);
            linearNormSum += norm;
            const size_t bin = std::min<size_t>(255, static_cast<size_t>(norm * 255.0));
            hist[bin]++;
        }
    }

    ettr.peakPhotositeCode = peakCode;
    ettr.clippedPhotositeRatio =
        static_cast<float>(static_cast<double>(clippedCount) / static_cast<double>(totalSamples));

    // Find 99.9th percentile bin for robust ETTR headroom in stops
    const uint64_t targetRank = static_cast<uint64_t>(static_cast<double>(totalSamples) * 0.999);
    uint64_t cumulative = 0;
    size_t p999Bin = 255;
    for (size_t b = 0; b < 256; ++b) {
        cumulative += hist[b];
        if (cumulative >= targetRank) {
            p999Bin = b;
            break;
        }
    }

    const double p999Norm = std::max(1.0 / 4096.0, (static_cast<double>(p999Bin) + 0.5) / 256.0);
    const int32_t avgBlack = (dynamicBlackLevelRggb[0] + dynamicBlackLevelRggb[1] +
                              dynamicBlackLevelRggb[2] + dynamicBlackLevelRggb[3]) / 4;
    ettr.p999PhotositeCode = static_cast<uint16_t>(
        avgBlack + p999Norm * std::max<int32_t>(1, whiteLevel - avgBlack));

    // Headroom in stops = -log2(p999Norm). 0.0 stops = at clipping point; +1.5 stops = 1.5 stops below whiteLevel.
    ettr.highlightHeadroomStops =
        static_cast<float>(std::max(0.0, -std::log2(std::min(1.0, p999Norm))));

    const double meanNorm = std::max(1e-5, linearNormSum / static_cast<double>(totalSamples));
    ettr.meanLinearExposureStops = static_cast<float>(std::log2(meanNorm));
    return ettr;
}

SensorCalibrationStats SensorCalibrationAndClarity::calibrateAndAnalyzePlanes(
    PlanarBayerView& planes,
    const uint16_t dynamicBlackLevelRggb[4],
    uint16_t whiteLevel,
    uint32_t currentIso,
    const lp::DeviceProfileRecord& deviceProfile) noexcept {
    SensorCalibrationStats stats{};
    const uint32_t w = planes.planeWidth;
    const uint32_t h = planes.planeHeight;
    if (w == 0 || h == 0 || planes.planeR == nullptr) {
        return stats;
    }

    float rBanding = 0.0f;
    float grBanding = 0.0f;
    float gbBanding = 0.0f;
    float bBanding = 0.0f;

    stats.badPixelsAndPdafRepaired += repairPlaneImpulsesAndBanding(
        planes.planeR, w, h, dynamicBlackLevelRggb[0], whiteLevel, true, rBanding);
    stats.badPixelsAndPdafRepaired += repairPlaneImpulsesAndBanding(
        planes.planeGr, w, h, dynamicBlackLevelRggb[1], whiteLevel, true, grBanding);
    stats.badPixelsAndPdafRepaired += repairPlaneImpulsesAndBanding(
        planes.planeGb, w, h, dynamicBlackLevelRggb[2], whiteLevel, true, gbBanding);
    stats.badPixelsAndPdafRepaired += repairPlaneImpulsesAndBanding(
        planes.planeB, w, h, dynamicBlackLevelRggb[3], whiteLevel, true, bBanding);

    stats.rowBandingRmsReduced = 0.25f * (rBanding + grBanding + gbBanding + bBanding);

    // 3. Green-split equilibration (Gr vs Gb imbalance correction to prevent maze demosaic artifacts)
    if (deviceProfile.quirkGreenSplitCorrectionRequired) {
        const size_t n = planes.samplesPerPlane();
        double diffAccum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const int32_t gr = static_cast<int32_t>(planes.planeGr[i]);
            const int32_t gb = static_cast<int32_t>(planes.planeGb[i]);
            const int32_t diff = gr - gb;
            diffAccum += std::abs(diff);

            // Equilibrate subtle crosstalk divergence while preserving sharp luminance edges
            if (std::abs(diff) < static_cast<int32_t>(whiteLevel / 8)) {
                const int32_t avg = (gr + gb + 1) >> 1;
                planes.planeGr[i] = static_cast<uint16_t>((gr + avg) >> 1);
                planes.planeGb[i] = static_cast<uint16_t>((gb + avg) >> 1);
            }
        }
        stats.greenSplitMeanDivergenceCorrected =
            static_cast<float>(diffAccum / static_cast<double>(n));
    }

    stats.ettr = computeEttrStops(planes, dynamicBlackLevelRggb, whiteLevel, currentIso, deviceProfile);
    return stats;
}

} // namespace apexcine::l2
