#pragma once

#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"
#include "apexcine/lp_platform/DeviceQuirkDatabase.hpp"
#include <cstdint>

namespace apexcine::l2 {

struct EttrExposureTelemetry {
    float highlightHeadroomStops{2.0f}; // Stops below WHITE_LEVEL for p99.9 photosites
    float clippedPhotositeRatio{0.0f};  // Ratio of photosites >= 99.5% of WHITE_LEVEL
    float meanLinearExposureStops{-3.0f};
    uint16_t peakPhotositeCode{0};
    uint16_t p999PhotositeCode{0};
    bool isDualConversionGainHigh{false};
    uint32_t recommendedDcgIso{400};
    float estimatedDynamicRangeStops{13.6f};
};

struct SensorCalibrationStats {
    uint32_t badPixelsAndPdafRepaired{0};
    float greenSplitMeanDivergenceCorrected{0.0f};
    float rowBandingRmsReduced{0.0f};
    EttrExposureTelemetry ettr{};
};

/**
 * Layer 2: Sensor Calibration, Defect Repair, Green-Split Equilibration & ETTR Intelligence.
 *
 * Corrects the physical sensor flaws in the linear CFA domain without destructive "ISP smoothing":
 * 1. Bad-pixel & PDAF dot defect repair (edge-aware median of same-CFA neighbors).
 * 2. Row/column Fixed-Pattern Noise (FPN) horizontal banding neutralization.
 * 3. Green-split (Gr vs Gb) local equilibration to prevent maze/zipper demosaic artifacts.
 * 4. True RAW ETTR (Expose To The Right) telemetry referenced to WHITE_LEVEL in stops.
 */
class SensorCalibrationAndClarity {
public:
    static SensorCalibrationStats calibrateAndAnalyzePlanes(
        PlanarBayerView& planes,
        const uint16_t dynamicBlackLevelRggb[4],
        uint16_t whiteLevel,
        uint32_t currentIso,
        const lp::DeviceProfileRecord& deviceProfile) noexcept;

    static EttrExposureTelemetry computeEttrStops(
        const PlanarBayerView& planes,
        const uint16_t dynamicBlackLevelRggb[4],
        uint16_t whiteLevel,
        uint32_t currentIso,
        const lp::DeviceProfileRecord& deviceProfile) noexcept;
};

} // namespace apexcine::l2
