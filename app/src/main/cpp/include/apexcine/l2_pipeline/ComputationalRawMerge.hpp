#pragma once

#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace apexcine::l2 {

struct BurstFrameInput {
    PlanarBayerView planes{};
    float gyroShiftPlaneX{0.0f}; // Initial gyro-predicted displacement in CFA plane pixels
    float gyroShiftPlaneY{0.0f};
    float exposureRatioToReference{1.0f}; // 1.0 for equal exposure burst; < 1.0 for underexposed HDR bracket
};

struct ComputationalRawMergeConfig {
    bool puristSingleFrameMode{false}; // True = 100% untouched single-frame RAW bypass
    float shotNoiseFactorK{1.8f};      // Poisson shot noise coefficient
    float readNoiseSigmaADU{14.0f};    // Gaussian read noise floor in ADU
    float motionRejectionTau{2.6f};    // Robust Tukey/Huber motion mask threshold in sigma units
    int32_t maxSearchRadiusPlanes{4};  // Local optical-flow refinement search window around gyro prior
    uint16_t blackLevel{256};
    uint16_t whiteLevel{4095};
};

struct ComputationalRawMergeResult {
    bool wasMerged{false};
    uint32_t framesFused{1};
    double shadowSnrBeforeDb{0.0};
    double shadowSnrAfterDb{0.0};
    double snrImprovementStops{0.0}; // 1 stop SNR = 6.0206 dB (2x amplitude / 4x variance reduction)
    float motionRejectedPixelRatio{0.0f};
    double maxGhostingArtifactErrorADU{0.0};
};

/**
 * Layer 2: Computational RAW — Multi-Frame Gyro+Optical-Flow Aligned Linear Bayer Merger.
 *
 * Operates strictly in the un-demosaiced 4-plane Bayer CFA domain:
 * 1. Uses gyro shift prior + 8x8 tile SAD block matching on the Green (Gr) plane to find exact
 *    CFA-aligned translation per tile.
 * 2. Evaluates a physical Poisson-Gaussian sensor noise model:
 *      var(y) = k_shot * max(y - blackLevel, 0) + sigma_read^2
 * 3. Computes robust motion-rejection weights:
 *      w = exp( - diff^2 / (2 * tau^2 * var(y)) )
 *    so moving subjects fall back cleanly to the reference frame with zero ghosting.
 * 4. Produces a single 16-bit linear RAW Bayer output frame with >= 2 stops SNR gain on 4-8 frame bursts.
 */
class ComputationalRawMerge {
public:
    static ComputationalRawMergeResult mergeBurstInRawDomain(
        const BurstFrameInput& referenceFrame,
        const std::vector<BurstFrameInput>& alternateFrames,
        const ComputationalRawMergeConfig& config,
        PlanarBayerView& outMergedPlanes) noexcept;

    /**
     * Computes empirical SNR (in dB) on a uniform patch of a CFA plane.
     */
    static double measurePatchSnrDb(const uint16_t* plane,
                                    uint32_t planeWidth,
                                    uint32_t roiX,
                                    uint32_t roiY,
                                    uint32_t roiW,
                                    uint32_t roiH,
                                    uint16_t blackLevel) noexcept;
};

} // namespace apexcine::l2
