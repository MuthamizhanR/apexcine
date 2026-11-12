#include "apexcine/l2_pipeline/ComputationalRawMerge.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace apexcine::l2 {

namespace {
constexpr uint32_t kTileSize = 8;

struct TileVector {
    int32_t dx{0};
    int32_t dy{0};
};

TileVector refineTileOpticalFlowOnGreen(const uint16_t* refGr,
                                        const uint16_t* altGr,
                                        uint32_t w,
                                        uint32_t h,
                                        uint32_t tileX0,
                                        uint32_t tileY0,
                                        int32_t priorDx,
                                        int32_t priorDy,
                                        int32_t searchRadius) noexcept {
    TileVector best{priorDx, priorDy};
    uint64_t bestSad = UINT64_MAX;

    const uint32_t txEnd = std::min(w, tileX0 + kTileSize);
    const uint32_t tyEnd = std::min(h, tileY0 + kTileSize);

    for (int32_t dy = priorDy - searchRadius; dy <= priorDy + searchRadius; ++dy) {
        for (int32_t dx = priorDx - searchRadius; dx <= priorDx + searchRadius; ++dx) {
            // Regularize toward IMU gyro prior so textureless shadow tiles do not overfit noise
            const uint64_t distFromPrior =
                static_cast<uint64_t>(std::abs(dx - priorDx) + std::abs(dy - priorDy));
            uint64_t sad = distFromPrior * static_cast<uint64_t>(kTileSize * kTileSize * 12);
            for (uint32_t y = tileY0; y < tyEnd; ++y) {
                const int32_t ay = std::clamp(static_cast<int32_t>(y) + dy, 0, static_cast<int32_t>(h) - 1);
                const uint16_t* refRow = refGr + static_cast<size_t>(y) * w;
                const uint16_t* altRow = altGr + static_cast<size_t>(ay) * w;
                for (uint32_t x = tileX0; x < txEnd; ++x) {
                    const int32_t ax = std::clamp(static_cast<int32_t>(x) + dx, 0, static_cast<int32_t>(w) - 1);
                    sad += static_cast<uint64_t>(
                        std::abs(static_cast<int32_t>(refRow[x]) - static_cast<int32_t>(altRow[ax])));
                }
            }
            if (sad < bestSad) {
                bestSad = sad;
                best = {dx, dy};
            }
        }
    }
    return best;
}
} // namespace

double ComputationalRawMerge::measurePatchSnrDb(const uint16_t* plane,
                                                uint32_t planeWidth,
                                                uint32_t roiX,
                                                uint32_t roiY,
                                                uint32_t roiW,
                                                uint32_t roiH,
                                                uint16_t blackLevel) noexcept {
    if (plane == nullptr || roiW == 0 || roiH == 0) return 0.0;
    const size_t count = static_cast<size_t>(roiW) * static_cast<size_t>(roiH);
    double sum = 0.0;
    for (uint32_t y = roiY; y < roiY + roiH; ++y) {
        const uint16_t* row = plane + static_cast<size_t>(y) * planeWidth;
        for (uint32_t x = roiX; x < roiX + roiW; ++x) {
            sum += std::max<double>(0.0, static_cast<double>(row[x]) - blackLevel);
        }
    }
    const double mean = sum / static_cast<double>(count);

    double varSum = 0.0;
    for (uint32_t y = roiY; y < roiY + roiH; ++y) {
        const uint16_t* row = plane + static_cast<size_t>(y) * planeWidth;
        for (uint32_t x = roiX; x < roiX + roiW; ++x) {
            const double v = std::max<double>(0.0, static_cast<double>(row[x]) - blackLevel);
            const double d = v - mean;
            varSum += d * d;
        }
    }
    const double stdDev = std::sqrt(std::max(1e-9, varSum / static_cast<double>(count)));
    if (mean <= 1e-6) return 0.0;
    return 20.0 * std::log10(mean / stdDev);
}

ComputationalRawMergeResult ComputationalRawMerge::mergeBurstInRawDomain(
    const BurstFrameInput& referenceFrame,
    const std::vector<BurstFrameInput>& alternateFrames,
    const ComputationalRawMergeConfig& config,
    PlanarBayerView& outMergedPlanes) noexcept {
    ComputationalRawMergeResult result{};
    const uint32_t w = referenceFrame.planes.planeWidth;
    const uint32_t h = referenceFrame.planes.planeHeight;
    const size_t n = referenceFrame.planes.samplesPerPlane();

    if (w == 0 || h == 0 || referenceFrame.planes.planeR == nullptr ||
        outMergedPlanes.planeR == nullptr) {
        return result;
    }

    // Measure baseline SNR on interior uniform patch of reference Green plane
    const uint32_t roiX = std::min<uint32_t>(4, w / 8);
    const uint32_t roiY = std::min<uint32_t>(4, h / 8);
    const uint32_t patchW = std::min<uint32_t>(24, w / 2);
    const uint32_t patchH = std::min<uint32_t>(24, h / 2);
    result.shadowSnrBeforeDb = measurePatchSnrDb(
        referenceFrame.planes.planeGr, w, roiX, roiY, patchW, patchH, config.blackLevel);

    // Purist Single-Frame RAW Mode: bit-exact copy of reference frame without temporal fusion
    if (config.puristSingleFrameMode || alternateFrames.empty()) {
        std::memcpy(outMergedPlanes.planeR, referenceFrame.planes.planeR, n * sizeof(uint16_t));
        std::memcpy(outMergedPlanes.planeGr, referenceFrame.planes.planeGr, n * sizeof(uint16_t));
        std::memcpy(outMergedPlanes.planeGb, referenceFrame.planes.planeGb, n * sizeof(uint16_t));
        std::memcpy(outMergedPlanes.planeB, referenceFrame.planes.planeB, n * sizeof(uint16_t));
        result.wasMerged = false;
        result.framesFused = 1;
        result.shadowSnrAfterDb = result.shadowSnrBeforeDb;
        result.snrImprovementStops = 0.0;
        return result;
    }

    const uint16_t* refPtrs[4] = {
        referenceFrame.planes.planeR,
        referenceFrame.planes.planeGr,
        referenceFrame.planes.planeGb,
        referenceFrame.planes.planeB};
    uint16_t* dstPtrs[4] = {
        outMergedPlanes.planeR,
        outMergedPlanes.planeGr,
        outMergedPlanes.planeGb,
        outMergedPlanes.planeB};

    // Precompute aligned tile motion vectors for each alternate frame
    const uint32_t tilesX = (w + kTileSize - 1) / kTileSize;
    const uint32_t tilesY = (h + kTileSize - 1) / kTileSize;
    std::vector<std::vector<TileVector>> frameTileVectors(alternateFrames.size());

    for (size_t f = 0; f < alternateFrames.size(); ++f) {
        const auto& alt = alternateFrames[f];
        frameTileVectors[f].resize(static_cast<size_t>(tilesX) * tilesY);
        const int32_t priorDx = static_cast<int32_t>(std::lround(alt.gyroShiftPlaneX));
        const int32_t priorDy = static_cast<int32_t>(std::lround(alt.gyroShiftPlaneY));

        for (uint32_t ty = 0; ty < tilesY; ++ty) {
            for (uint32_t tx = 0; tx < tilesX; ++tx) {
                frameTileVectors[f][static_cast<size_t>(ty) * tilesX + tx] =
                    refineTileOpticalFlowOnGreen(referenceFrame.planes.planeGr,
                                                 alt.planes.planeGr,
                                                 w,
                                                 h,
                                                 tx * kTileSize,
                                                 ty * kTileSize,
                                                 priorDx,
                                                 priorDy,
                                                 config.maxSearchRadiusPlanes);
            }
        }
    }

    const float readVar = config.readNoiseSigmaADU * config.readNoiseSigmaADU;
    const float tau2 = config.motionRejectionTau * config.motionRejectionTau;
    uint64_t motionRejectedCount = 0;
    const uint64_t totalCandidateEvaluations =
        static_cast<uint64_t>(n) * 4 * alternateFrames.size();

    // Fuse each of the 4 Bayer planes in linear RAW space with Poisson-Gaussian robust weights
    for (int p = 0; p < 4; ++p) {
        const uint16_t* refPlane = refPtrs[p];
        uint16_t* outPlane = dstPtrs[p];

        for (uint32_t y = 0; y < h; ++y) {
            const uint32_t ty = y / kTileSize;
            for (uint32_t x = 0; x < w; ++x) {
                const uint32_t tx = x / kTileSize;
                const size_t tileIdx = static_cast<size_t>(ty) * tilesX + tx;
                const size_t pixIdx = static_cast<size_t>(y) * w + x;

                const float refCode = static_cast<float>(refPlane[pixIdx]);
                const float refSignal = std::max(0.0f, refCode - static_cast<float>(config.blackLevel));
                // Poisson-Gaussian variance at reference pixel
                const float noiseVar = std::max(1.0f, config.shotNoiseFactorK * refSignal + readVar);

                double weightedSum = static_cast<double>(refCode);
                double weightTotal = 1.0;

                for (size_t f = 0; f < alternateFrames.size(); ++f) {
                    const auto& alt = alternateFrames[f];
                    const uint16_t* altPlane =
                        (p == 0) ? alt.planes.planeR
                        : (p == 1) ? alt.planes.planeGr
                        : (p == 2) ? alt.planes.planeGb
                                   : alt.planes.planeB;

                    const TileVector mv = frameTileVectors[f][tileIdx];
                    const int32_t ax =
                        std::clamp(static_cast<int32_t>(x) + mv.dx, 0, static_cast<int32_t>(w) - 1);
                    const int32_t ay =
                        std::clamp(static_cast<int32_t>(y) + mv.dy, 0, static_cast<int32_t>(h) - 1);

                    float altCode =
                        static_cast<float>(altPlane[static_cast<size_t>(ay) * w + ax]);

                    // Normalize exposure if merging an underexposed HDR bracket frame
                    if (std::abs(alt.exposureRatioToReference - 1.0f) > 1e-3f &&
                        alt.exposureRatioToReference > 1e-3f) {
                        const float altSig =
                            std::max(0.0f, altCode - static_cast<float>(config.blackLevel));
                        altCode = static_cast<float>(config.blackLevel) +
                                  (altSig / alt.exposureRatioToReference);
                    }

                    // Also measure 3x3 neighborhood difference to robustly reject moving object boundaries
                    float neighborhoodDiff2 = (altCode - refCode) * (altCode - refCode);
                    if (x > 0 && x + 1 < w) {
                        const float rLeft = static_cast<float>(refPlane[pixIdx - 1]);
                        const float aLeft = static_cast<float>(
                            altPlane[static_cast<size_t>(ay) * w + std::max(0, ax - 1)]);
                        neighborhoodDiff2 =
                            0.6f * neighborhoodDiff2 + 0.4f * ((aLeft - rLeft) * (aLeft - rLeft));
                    }

                    const float z2 = neighborhoodDiff2 / (2.0f * tau2 * noiseVar);
                    if (z2 > 2.5f) {
                        ++motionRejectedCount;
                        continue; // Reject ghosting pixel completely
                    }
                    // Flat-top robust kernel: full weight 1.0 within stationary noise band (z2 <= 1.0)
                    // so stationary variance reduces by 1/N without biasing toward reference noise
                    const double wAlt =
                        (z2 <= 1.0f) ? 1.0 : std::exp(-static_cast<double>(z2 - 1.0f));
                    weightedSum += wAlt * static_cast<double>(altCode);
                    weightTotal += wAlt;
                }

                const int32_t mergedVal = static_cast<int32_t>(std::lround(weightedSum / weightTotal));
                outPlane[pixIdx] = static_cast<uint16_t>(
                    std::clamp<int32_t>(mergedVal, 0, config.whiteLevel));
            }
        }
    }

    result.wasMerged = true;
    result.framesFused = static_cast<uint32_t>(1 + alternateFrames.size());
    result.shadowSnrAfterDb = measurePatchSnrDb(
        outMergedPlanes.planeGr, w, roiX, roiY, patchW, patchH, config.blackLevel);
    // Amplitude SNR is 20 * log10(signal / sigma_noise).
    // Reducing RMS noise sigma by 2x (+1 stop dynamic range) requires +6.0206 dB (N=4 frames);
    // reducing RMS noise sigma by 4x (+2 stops dynamic range) requires +12.0412 dB (N=16 frames).
    result.snrImprovementStops =
        std::max(0.0, (result.shadowSnrAfterDb - result.shadowSnrBeforeDb) / 6.02059991328);
    result.motionRejectedPixelRatio =
        (totalCandidateEvaluations > 0)
            ? static_cast<float>(static_cast<double>(motionRejectedCount) /
                                 static_cast<double>(totalCandidateEvaluations))
            : 0.0f;
    return result;
}

} // namespace apexcine::l2
