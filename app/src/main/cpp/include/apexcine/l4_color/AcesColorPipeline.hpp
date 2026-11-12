#pragma once

#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"
#include "apexcine/lp_platform/DeviceQuirkDatabase.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace apexcine::l4 {

struct LabColor {
    double L{0.0};
    double a{0.0};
    double b{0.0};
};

struct ColorCheckerAccuracyReport {
    bool isCalibrated{false};
    double meanDeltaE2000_D50{-1.0};
    double maxDeltaE2000_D50{-1.0};
    double meanDeltaE2000_StdA{-1.0};
    double maxDeltaE2000_StdA{-1.0};
    bool meetsTargetKpiUnder3{false};
};

/**
 * Layer 4: Dual-Illuminant Color Science, Linear AP1 Working Space & ACEScct Pipeline.
 *
 * - Interpolates Forward Matrices between Illuminant A (2856K) and D65 (6504K) in mired (1/T) space.
 * - Concatenates Camera RGB -> CIE XYZ (D50) -> Bradford D50-to-D60 CAT -> ACES AP1 linear working space.
 * - Implements official AMPAS S-2016-001 ACEScct logarithmic transfer function and inverse.
 * - Applies half-res superpixel preview debayer + linearly-interpolated LUT + independent per-channel
 *   TPDF triangular dithering into 10-bit A2B10G10R10 output.
 * - Computes exact CIE Delta E 2000 (CIEDE2000) from measured camera RGB patches through the forward matrix.
 */
class AcesColorPipeline {
public:
    static float linearAp1ToAcescct(float linear) noexcept;
    static float acescctToLinearAp1(float cct) noexcept;

    /**
     * Exact CIE XYZ (D50) -> ACES AP1 (D60) 3x3 matrix (Bradford D50->D60 CAT * XYZ_D60->AP1).
     */
    static const std::array<float, 9>& xyzD50ToAcesAp1Matrix() noexcept;

    /**
     * Interpolates dual-illuminant forward matrices by correlated color temperature (Kelvin)
     * and premultiplies by CIE XYZ(D50) -> ACES AP1 matrix.
     */
    static void computeCameraToAp1Matrix(const lp::DualIlluminantColorProfile& profile,
                                         float cctKelvin,
                                         float outMatrix3x3[9]) noexcept;

    /**
     * Fast half-resolution superpixel preview debayer + WB + Camera-to-AP1 + linearly interpolated
     * ACEScct LUT + independent per-channel TPDF dither to 10-bit packed RGBA_1010102.
     */
    static void debayerAndGradeTo10BitAcescct(const l2::PlanarBayerView& planes,
                                              const uint16_t blackLevelRggb[4],
                                              uint16_t whiteLevel,
                                              const float wbGainsRggb[4],
                                              const float camToAp1Matrix[9],
                                              uint32_t* dstPacked1010102) noexcept;

    static LabColor xyzD50ToLab(double X, double Y, double Z) noexcept;
    static void labToXyzD50(const LabColor& lab, double outXyz[3]) noexcept;
    static const std::array<LabColor, 24>& referenceColorChecker24D50() noexcept;

    /**
     * Exact CIE Delta E 2000 (CIEDE2000) color difference formula between two CIE L*a*b* colors.
     */
    static double computeDeltaE2000(const LabColor& lab1, const LabColor& lab2) noexcept;

    /**
     * Evaluates 24-patch ColorChecker accuracy by transforming linear Camera RGB patch measurements
     * under D50 (5000K) and Illuminant A (2856K) through the profile's forward matrices into
     * CIE XYZ (D50) -> CIE L*a*b* -> CIEDE2000 against BabelColor reference patches.
     */
    static ColorCheckerAccuracyReport evaluateColorChecker24FromCameraRgb(
        const float camRgbPatchesD50[24][3],
        const float camRgbPatchesStdA[24][3],
        const lp::DualIlluminantColorProfile& profile) noexcept;

    /**
     * Evaluates a device profile's calibration status. If the profile does not have measured
     * chart patches (isColorimetricallyCalibrated == false), returns isCalibrated = false.
     */
    static ColorCheckerAccuracyReport evaluateColorChecker24(
        const lp::DualIlluminantColorProfile& profile) noexcept;

    /**
     * Generates an OpenColorIO (OCIO v2) config snippet and ACES IDT transform for NLE matching.
     */
    static std::string generateOcioConfigForDevice(const lp::DeviceProfileRecord& device);
};

} // namespace apexcine::l4
