#include "apexcine/l4_color/AcesColorPipeline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace apexcine::l4 {

namespace {
constexpr double kD50WhiteXyz[3] = {0.96422, 1.00000, 0.82521};
constexpr double kAcesD60WhiteXyz[3] = {0.95264607, 1.00000000, 1.00882518};

void interpolateForwardMatrixD50(const lp::DualIlluminantColorProfile& profile,
                                 float cctKelvin,
                                 float outFm[9]) noexcept {
    const float clampedK = std::clamp(cctKelvin, 2000.0f, 12000.0f);
    const float invT = 1.0f / clampedK;
    const float invT1 = 1.0f / static_cast<float>(profile.illuminant1Kelvin);
    const float invT2 = 1.0f / static_cast<float>(profile.illuminant2Kelvin);

    float alpha = 0.5f;
    if (std::abs(invT2 - invT1) > 1e-7f) {
        alpha = std::clamp((invT - invT1) / (invT2 - invT1), 0.0f, 1.0f);
    }

    for (int i = 0; i < 9; ++i) {
        outFm[i] = (1.0f - alpha) * profile.forwardMatrix1_XYZ_D50[i] +
                   alpha * profile.forwardMatrix2_XYZ_D50[i];
    }
}
} // namespace

const std::array<float, 9>& AcesColorPipeline::xyzD50ToAcesAp1Matrix() noexcept {
    // Computed once from exact Bradford cone response matrices M_A, M_A^-1, D50->D60 white points,
    // and official AMPAS S-2014-004 XYZ(D60)->AP1 matrix so D50 white (0.96422, 1.0, 0.82521)
    // maps to neutral AP1 (1.0, 1.0, 1.0).
    static const std::array<float, 9> kMatrix = []() {
        constexpr double mBradford[9] = {
             0.8951,  0.2664, -0.1614,
            -0.7502,  1.7135,  0.0367,
             0.0389, -0.0685,  1.0296
        };
        constexpr double mBradfordInv[9] = {
             0.9869929055, -0.1470542564,  0.1599626517,
             0.4323052697,  0.5183602715,  0.0492912282,
            -0.0085286646,  0.0400428217,  0.9684866958
        };
        // Exact inverse of AMPAS AP1-to-XYZ(D60) primaries with D60 white (0.95264607, 1.0, 1.00882518)
        constexpr double mXyzD60ToAp1[9] = {
             1.6410233797, -0.3248032942, -0.2364246952,
            -0.6636628587,  1.6153315917,  0.0167563477,
             0.0117218943, -0.0082844420,  0.9883948585
        };

        double rhoSrc[3]{}, rhoDst[3]{};
        for (int r = 0; r < 3; ++r) {
            rhoSrc[r] = mBradford[r * 3 + 0] * kD50WhiteXyz[0] +
                        mBradford[r * 3 + 1] * kD50WhiteXyz[1] +
                        mBradford[r * 3 + 2] * kD50WhiteXyz[2];
            rhoDst[r] = mBradford[r * 3 + 0] * kAcesD60WhiteXyz[0] +
                        mBradford[r * 3 + 1] * kAcesD60WhiteXyz[1] +
                        mBradford[r * 3 + 2] * kAcesD60WhiteXyz[2];
        }
        const double scale[3] = {rhoDst[0] / rhoSrc[0], rhoDst[1] / rhoSrc[1], rhoDst[2] / rhoSrc[2]};

        double diagBradford[9]{};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                diagBradford[r * 3 + c] = scale[r] * mBradford[r * 3 + c];
            }
        }

        double catD50ToD60[9]{};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                catD50ToD60[r * 3 + c] =
                    mBradfordInv[r * 3 + 0] * diagBradford[0 * 3 + c] +
                    mBradfordInv[r * 3 + 1] * diagBradford[1 * 3 + c] +
                    mBradfordInv[r * 3 + 2] * diagBradford[2 * 3 + c];
            }
        }

        std::array<float, 9> out{};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                const double v =
                    mXyzD60ToAp1[r * 3 + 0] * catD50ToD60[0 * 3 + c] +
                    mXyzD60ToAp1[r * 3 + 1] * catD50ToD60[1 * 3 + c] +
                    mXyzD60ToAp1[r * 3 + 2] * catD50ToD60[2 * 3 + c];
                out[r * 3 + c] = static_cast<float>(v);
            }
        }
        return out;
    }();
    return kMatrix;
}

float AcesColorPipeline::linearAp1ToAcescct(float linear) noexcept {
    // Official AMPAS S-2016-001 ACEScct encoding
    constexpr float kCutLinear = 0.0078125f; // 2^-7
    constexpr float kA = 10.5402377416545f;
    constexpr float kB = 0.0729055341958355f;
    if (linear <= kCutLinear) {
        return kA * linear + kB;
    }
    return (std::log2(linear) + 9.72f) / 17.52f;
}

float AcesColorPipeline::acescctToLinearAp1(float cct) noexcept {
    // Official AMPAS S-2016-001 ACEScct inverse
    constexpr float kCutCct = 0.155251141552511f;
    constexpr float kA = 10.5402377416545f;
    constexpr float kB = 0.0729055341958355f;
    if (cct <= kCutCct) {
        return (cct - kB) / kA;
    }
    if (cct >= (std::log2(65504.0f) + 9.72f) / 17.52f) {
        return 65504.0f;
    }
    return std::exp2(cct * 17.52f - 9.72f);
}

void AcesColorPipeline::computeCameraToAp1Matrix(const lp::DualIlluminantColorProfile& profile,
                                                 float cctKelvin,
                                                 float outMatrix3x3[9]) noexcept {
    float fmInterp[9]{};
    interpolateForwardMatrixD50(profile, cctKelvin, fmInterp);

    const auto& xyzD50ToAp1 = xyzD50ToAcesAp1Matrix();
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            outMatrix3x3[r * 3 + c] =
                xyzD50ToAp1[r * 3 + 0] * fmInterp[0 * 3 + c] +
                xyzD50ToAp1[r * 3 + 1] * fmInterp[1 * 3 + c] +
                xyzD50ToAp1[r * 3 + 2] * fmInterp[2 * 3 + c];
        }
    }
}

void AcesColorPipeline::debayerAndGradeTo10BitAcescct(const l2::PlanarBayerView& planes,
                                                      const uint16_t blackLevelRggb[4],
                                                      uint16_t whiteLevel,
                                                      const float wbGainsRggb[4],
                                                      const float camToAp1Matrix[9],
                                                      uint32_t* dstPacked1010102) noexcept {
    if (planes.planeR == nullptr || dstPacked1010102 == nullptr) return;
    const uint32_t w = planes.planeWidth;
    const uint32_t h = planes.planeHeight;

    // Precompute 4096-entry linear [0..2.0] -> 10-bit scaled ACEScct [0..1023] float table once
    static const std::array<float, 4096> kAcescctLut = []() {
        std::array<float, 4096> lut{};
        for (size_t i = 0; i < 4096; ++i) {
            const float lin = (static_cast<float>(i) / 4095.0f) * 2.0f;
            lut[i] = std::clamp(linearAp1ToAcescct(lin), 0.0f, 1.0f) * 1023.0f;
        }
        return lut;
    }();

    auto sampleLutLinear = [](float ap1Val) noexcept -> float {
        const float pos = std::clamp(ap1Val, 0.0f, 2.0f) * 2047.5f;
        const size_t idx0 = static_cast<size_t>(pos);
        const size_t idx1 = std::min<size_t>(idx0 + 1, 4095);
        const float frac = pos - static_cast<float>(idx0);
        return kAcescctLut[idx0] + frac * (kAcescctLut[idx1] - kAcescctLut[idx0]);
    };

    const float scaleR = wbGainsRggb[0] / std::max<float>(1.0f, whiteLevel - blackLevelRggb[0]);
    const float scaleGr = wbGainsRggb[1] / std::max<float>(1.0f, whiteLevel - blackLevelRggb[1]);
    const float scaleGb = wbGainsRggb[2] / std::max<float>(1.0f, whiteLevel - blackLevelRggb[2]);
    const float scaleB = wbGainsRggb[3] / std::max<float>(1.0f, whiteLevel - blackLevelRggb[3]);

    const float blR = static_cast<float>(blackLevelRggb[0]);
    const float blGr = static_cast<float>(blackLevelRggb[1]);
    const float blGb = static_cast<float>(blackLevelRggb[2]);
    const float blB = static_cast<float>(blackLevelRggb[3]);

    constexpr float kTpdfScale = 1.0f / 255.0f; // [-1.0, +1.0] LSB triangular PDF

    for (uint32_t y = 0; y < h; ++y) {
        const size_t rowOff = static_cast<size_t>(y) * w;
        uint32_t rng = y * 747796405u + 2891336453u;
        for (uint32_t x = 0; x < w; ++x) {
            const size_t idx = rowOff + x;
            const float rLin = std::max(0.0f, (static_cast<float>(planes.planeR[idx]) - blR) * scaleR);
            const float gLin = 0.5f * (
                std::max(0.0f, (static_cast<float>(planes.planeGr[idx]) - blGr) * scaleGr) +
                std::max(0.0f, (static_cast<float>(planes.planeGb[idx]) - blGb) * scaleGb));
            const float bLin = std::max(0.0f, (static_cast<float>(planes.planeB[idx]) - blB) * scaleB);

            const float ap1R = camToAp1Matrix[0] * rLin + camToAp1Matrix[1] * gLin + camToAp1Matrix[2] * bLin;
            const float ap1G = camToAp1Matrix[3] * rLin + camToAp1Matrix[4] * gLin + camToAp1Matrix[5] * bLin;
            const float ap1B = camToAp1Matrix[6] * rLin + camToAp1Matrix[7] * gLin + camToAp1Matrix[8] * bLin;

            const float cctR10 = sampleLutLinear(ap1R);
            const float cctG10 = sampleLutLinear(ap1G);
            const float cctB10 = sampleLutLinear(ap1B);

            // Independent per-channel TPDF triangular dither in [-1.0, +1.0] LSB
            rng = rng * 1664525u + 1013904223u;
            const float ditherR = (static_cast<float>(rng & 0xFFu) -
                                   static_cast<float>((rng >> 8) & 0xFFu)) * kTpdfScale;
            const float ditherG = (static_cast<float>((rng >> 16) & 0xFFu) -
                                   static_cast<float>((rng >> 24) & 0xFFu)) * kTpdfScale;
            rng = rng * 1664525u + 1013904223u;
            const float ditherB = (static_cast<float>(rng & 0xFFu) -
                                   static_cast<float>((rng >> 8) & 0xFFu)) * kTpdfScale;

            const uint32_t r10 = static_cast<uint32_t>(
                std::clamp(static_cast<int32_t>(cctR10 + ditherR + 0.5f), 0, 1023));
            const uint32_t g10 = static_cast<uint32_t>(
                std::clamp(static_cast<int32_t>(cctG10 + ditherG + 0.5f), 0, 1023));
            const uint32_t b10 = static_cast<uint32_t>(
                std::clamp(static_cast<int32_t>(cctB10 + ditherB + 0.5f), 0, 1023));

            dstPacked1010102[idx] = r10 | (g10 << 10) | (b10 << 20) | (3u << 30);
        }
    }
}

LabColor AcesColorPipeline::xyzD50ToLab(double X, double Y, double Z) noexcept {
    constexpr double kEps = 216.0 / 24389.0;
    constexpr double kKappa = 24389.0 / 27.0;
    auto f = [](double t) noexcept -> double {
        return (t > kEps) ? std::cbrt(t) : ((kKappa * t + 16.0) / 116.0);
    };
    const double fx = f(X / kD50WhiteXyz[0]);
    const double fy = f(Y / kD50WhiteXyz[1]);
    const double fz = f(Z / kD50WhiteXyz[2]);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

void AcesColorPipeline::labToXyzD50(const LabColor& lab, double outXyz[3]) noexcept {
    constexpr double kEps = 216.0 / 24389.0;
    constexpr double kKappa = 24389.0 / 27.0;
    const double fy = (lab.L + 16.0) / 116.0;
    const double fx = fy + (lab.a / 500.0);
    const double fz = fy - (lab.b / 200.0);
    auto finv = [](double fval) noexcept -> double {
        const double f3 = fval * fval * fval;
        return (f3 > kEps) ? f3 : ((116.0 * fval - 16.0) / kKappa);
    };
    outXyz[0] = kD50WhiteXyz[0] * finv(fx);
    outXyz[1] = kD50WhiteXyz[1] * finv(fy);
    outXyz[2] = kD50WhiteXyz[2] * finv(fz);
}

const std::array<LabColor, 24>& AcesColorPipeline::referenceColorChecker24D50() noexcept {
    // 24 ColorChecker Classic reference CIE L*a*b* D50 coordinates (X-Rite / BabelColor reference)
    static constexpr std::array<LabColor, 24> kRefPatchesD50 = {{
        {37.986,  13.555,  14.059}, // 1. Dark Skin
        {65.711,  18.130,  17.810}, // 2. Light Skin
        {49.927,  -4.880, -21.925}, // 3. Blue Sky
        {43.139, -13.095,  21.905}, // 4. Foliage
        {55.112,   8.844, -25.399}, // 5. Blue Flower
        {70.719, -33.397,  -0.199}, // 6. Bluish Green
        {62.661,  36.067,  57.096}, // 7. Orange
        {40.020,  10.410, -45.964}, // 8. Purplish Blue
        {51.124,  48.239,  16.248}, // 9. Moderate Red
        {30.325,  22.976, -21.587}, // 10. Purple
        {72.532, -23.709,  57.255}, // 11. Yellow Green
        {71.941,  19.363,  67.857}, // 12. Orange Yellow
        {28.778,  14.179, -50.297}, // 13. Blue
        {55.261, -38.342,  31.370}, // 14. Green
        {42.101,  53.378,  28.190}, // 15. Red
        {81.733,   4.039,  79.819}, // 16. Yellow
        {51.935,  49.986, -14.574}, // 17. Magenta
        {51.038, -28.631, -28.638}, // 18. Cyan
        {96.539,  -0.425,   1.186}, // 19. White 9.5
        {81.257,  -0.638,  -0.335}, // 20. Neutral 8
        {66.766,  -0.734,  -0.504}, // 21. Neutral 6.5
        {50.867,  -0.153,  -0.270}, // 22. Neutral 5
        {35.656,  -0.421,  -1.231}, // 23. Neutral 3.5
        {20.461,  -0.079,  -0.973}  // 24. Black 2
    }};
    return kRefPatchesD50;
}

double AcesColorPipeline::computeDeltaE2000(const LabColor& lab1, const LabColor& lab2) noexcept {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kDeg2Rad = kPi / 180.0;
    constexpr double kRad2Deg = 180.0 / kPi;

    const double L1 = lab1.L, a1 = lab1.a, b1 = lab1.b;
    const double L2 = lab2.L, a2 = lab2.a, b2 = lab2.b;

    const double C1 = std::hypot(a1, b1);
    const double C2 = std::hypot(a2, b2);
    const double Cbar = 0.5 * (C1 + C2);
    const double Cbar7 = std::pow(Cbar, 7.0);
    const double G = 0.5 * (1.0 - std::sqrt(Cbar7 / (Cbar7 + 6103515625.0))); // 25^7

    const double a1p = (1.0 + G) * a1;
    const double a2p = (1.0 + G) * a2;
    const double C1p = std::hypot(a1p, b1);
    const double C2p = std::hypot(a2p, b2);

    auto hueAngleDeg = [](double ap, double b) noexcept -> double {
        if (ap == 0.0 && b == 0.0) return 0.0;
        double h = std::atan2(b, ap) * kRad2Deg;
        return (h < 0.0) ? (h + 360.0) : h;
    };
    const double h1p = hueAngleDeg(a1p, b1);
    const double h2p = hueAngleDeg(a2p, b2);

    const double dLp = L2 - L1;
    const double dCp = C2p - C1p;

    double dhp = 0.0;
    if (C1p * C2p != 0.0) {
        if (std::abs(h2p - h1p) <= 180.0) {
            dhp = h2p - h1p;
        } else if (h2p - h1p > 180.0) {
            dhp = (h2p - h1p) - 360.0;
        } else {
            dhp = (h2p - h1p) + 360.0;
        }
    }
    const double dHp = 2.0 * std::sqrt(C1p * C2p) * std::sin(0.5 * dhp * kDeg2Rad);

    const double Lbarp = 0.5 * (L1 + L2);
    const double Cbarp = 0.5 * (C1p + C2p);

    double hbarp = h1p + h2p;
    if (C1p * C2p != 0.0) {
        if (std::abs(h1p - h2p) <= 180.0) {
            hbarp = 0.5 * (h1p + h2p);
        } else if (h1p + h2p < 360.0) {
            hbarp = 0.5 * (h1p + h2p + 360.0);
        } else {
            hbarp = 0.5 * (h1p + h2p - 360.0);
        }
    }

    const double T = 1.0 - 0.17 * std::cos((hbarp - 30.0) * kDeg2Rad) +
                     0.24 * std::cos((2.0 * hbarp) * kDeg2Rad) +
                     0.32 * std::cos((3.0 * hbarp + 6.0) * kDeg2Rad) -
                     0.20 * std::cos((4.0 * hbarp - 63.0) * kDeg2Rad);

    const double dTheta = 30.0 * std::exp(-std::pow((hbarp - 275.0) / 25.0, 2.0));
    const double Cbarp7 = std::pow(Cbarp, 7.0);
    const double RC = 2.0 * std::sqrt(Cbarp7 / (Cbarp7 + 6103515625.0));

    const double LbarpMinus50Sq = (Lbarp - 50.0) * (Lbarp - 50.0);
    const double SL = 1.0 + (0.015 * LbarpMinus50Sq) / std::sqrt(20.0 + LbarpMinus50Sq);
    const double SC = 1.0 + 0.045 * Cbarp;
    const double SH = 1.0 + 0.015 * Cbarp * T;
    const double RT = -std::sin(2.0 * dTheta * kDeg2Rad) * RC;

    const double termL = dLp / SL;
    const double termC = dCp / SC;
    const double termH = dHp / SH;

    return std::sqrt(termL * termL + termC * termC + termH * termH + RT * termC * termH);
}

ColorCheckerAccuracyReport AcesColorPipeline::evaluateColorChecker24FromCameraRgb(
    const float camRgbPatchesD50[24][3],
    const float camRgbPatchesStdA[24][3],
    const lp::DualIlluminantColorProfile& profile) noexcept {
    ColorCheckerAccuracyReport rep{};
    if (camRgbPatchesD50 == nullptr || camRgbPatchesStdA == nullptr) {
        return rep;
    }
    rep.isCalibrated = true;
    rep.maxDeltaE2000_D50 = 0.0;
    rep.maxDeltaE2000_StdA = 0.0;

    float fmD50[9]{};
    float fmStdA[9]{};
    interpolateForwardMatrixD50(profile, 5003.0f, fmD50);
    interpolateForwardMatrixD50(profile, static_cast<float>(profile.illuminant1Kelvin), fmStdA);

    const auto& refs = referenceColorChecker24D50();
    double sumD50 = 0.0;
    double sumStdA = 0.0;

    for (size_t i = 0; i < 24; ++i) {
        const float* rgbD50 = camRgbPatchesD50[i];
        const double xD50 = fmD50[0] * rgbD50[0] + fmD50[1] * rgbD50[1] + fmD50[2] * rgbD50[2];
        const double yD50 = fmD50[3] * rgbD50[0] + fmD50[4] * rgbD50[1] + fmD50[5] * rgbD50[2];
        const double zD50 = fmD50[6] * rgbD50[0] + fmD50[7] * rgbD50[1] + fmD50[8] * rgbD50[2];
        const LabColor labD50 = xyzD50ToLab(xD50, yD50, zD50);

        const float* rgbA = camRgbPatchesStdA[i];
        const double xA = fmStdA[0] * rgbA[0] + fmStdA[1] * rgbA[1] + fmStdA[2] * rgbA[2];
        const double yA = fmStdA[3] * rgbA[0] + fmStdA[4] * rgbA[1] + fmStdA[5] * rgbA[2];
        const double zA = fmStdA[6] * rgbA[0] + fmStdA[7] * rgbA[1] + fmStdA[8] * rgbA[2];
        const LabColor labStdA = xyzD50ToLab(xA, yA, zA);

        const double deD50 = computeDeltaE2000(refs[i], labD50);
        const double deStdA = computeDeltaE2000(refs[i], labStdA);
        sumD50 += deD50;
        sumStdA += deStdA;
        rep.maxDeltaE2000_D50 = std::max(rep.maxDeltaE2000_D50, deD50);
        rep.maxDeltaE2000_StdA = std::max(rep.maxDeltaE2000_StdA, deStdA);
    }

    rep.meanDeltaE2000_D50 = sumD50 / 24.0;
    rep.meanDeltaE2000_StdA = sumStdA / 24.0;
    rep.meetsTargetKpiUnder3 =
        (rep.meanDeltaE2000_D50 < 3.0) && (rep.meanDeltaE2000_StdA < 3.0);
    return rep;
}

ColorCheckerAccuracyReport AcesColorPipeline::evaluateColorChecker24(
    const lp::DualIlluminantColorProfile& profile) noexcept {
    ColorCheckerAccuracyReport rep{};
    if (!profile.isColorimetricallyCalibrated ||
        profile.measuredMeanDeltaE2000_D50 < 0.0f ||
        profile.measuredMeanDeltaE2000_StdA < 0.0f) {
        rep.isCalibrated = false;
        rep.meanDeltaE2000_D50 = -1.0;
        rep.maxDeltaE2000_D50 = -1.0;
        rep.meanDeltaE2000_StdA = -1.0;
        rep.maxDeltaE2000_StdA = -1.0;
        rep.meetsTargetKpiUnder3 = false;
        return rep;
    }
    rep.isCalibrated = true;
    rep.meanDeltaE2000_D50 = profile.measuredMeanDeltaE2000_D50;
    rep.meanDeltaE2000_StdA = profile.measuredMeanDeltaE2000_StdA;
    rep.meetsTargetKpiUnder3 =
        (rep.meanDeltaE2000_D50 < 3.0) && (rep.meanDeltaE2000_StdA < 3.0);
    return rep;
}

std::string AcesColorPipeline::generateOcioConfigForDevice(const lp::DeviceProfileRecord& device) {
    float matD55[9]{};
    computeCameraToAp1Matrix(device.colorProfile, 5500.0f, matD55);

    std::ostringstream oss;
    oss << "ocio_profile_version: 2\n"
        << "name: ApexCine_" << device.profileId << "_ACES_v1\n"
        << "description: Dual-Illuminant ACES IDT & ACEScct working space for "
        << device.marketingName
        << (device.colorProfile.isColorimetricallyCalibrated
                ? " (Chart Calibrated)\n"
                : " (Unverified Reference Matrix)\n")
        << "roles:\n"
        << "  scene_linear: ACEScg\n"
        << "  color_timing: ACEScct\n"
        << "colorspaces:\n"
        << "  - !<ColorSpace>\n"
        << "    name: IDT_" << device.profileId << "_Linear\n"
        << "    family: Input/" << device.manufacturer << "\n"
        << "    to_scene_reference: !<MatrixTransform> {matrix: ["
        << matD55[0] << ", " << matD55[1] << ", " << matD55[2] << ", 0, "
        << matD55[3] << ", " << matD55[4] << ", " << matD55[5] << ", 0, "
        << matD55[6] << ", " << matD55[7] << ", " << matD55[8] << ", 0, "
        << "0, 0, 0, 1]}\n";
    return oss.str();
}

} // namespace apexcine::l4
