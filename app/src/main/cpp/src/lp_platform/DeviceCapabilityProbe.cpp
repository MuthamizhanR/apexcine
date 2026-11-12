#include "apexcine/lp_platform/DeviceCapabilityProbe.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

#if defined(__ANDROID__)
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#endif

namespace apexcine::lp {

std::string CameraDeviceCapabilityReport::toJson() const {
    std::ostringstream oss;
    oss << "{\n"
        << "  \"logicalCameraId\": \"" << logicalCameraId << "\",\n"
        << "  \"sensorModelTag\": \"" << sensorModelTag << "\",\n"
        << "  \"isSimulatedReport\": " << (isSimulatedReport ? "true" : "false") << ",\n"
        << "  \"supportsRawCapability\": " << (supportsRawCapability ? "true" : "false") << ",\n"
        << "  \"supportsManualSensor\": " << (supportsManualSensor ? "true" : "false") << ",\n"
        << "  \"supportsLogicalMultiCamera\": " << (supportsLogicalMultiCamera ? "true" : "false") << ",\n"
        << "  \"supportsReadoutTimestamp\": " << (supportsReadoutTimestamp ? "true" : "false") << ",\n"
        << "  \"timestampSourceRealtime\": " << (timestampSourceRealtime ? "true" : "false") << ",\n"
        << "  \"gpuHardwareBufferRawImportSupported\": "
        << (gpuHardwareBufferRawImportSupported ? "true" : "false") << ",\n"
        << "  \"cpuMapFallbackRequired\": " << (cpuMapFallbackRequired ? "true" : "false") << ",\n"
        << "  \"cfaSupported\": " << (cfaSupported ? "true" : "false") << ",\n"
        << "  \"cfaPattern\": " << static_cast<int>(cfaPattern) << ",\n"
        << "  \"sensorWhiteLevel\": " << sensorWhiteLevel << ",\n"
        << "  \"staticBlackLevelRggb\": [" << staticBlackLevelRggb[0] << ", "
        << staticBlackLevelRggb[1] << ", " << staticBlackLevelRggb[2] << ", "
        << staticBlackLevelRggb[3] << "],\n"
        << "  \"isoRange\": [" << isoRangeMin << ", " << isoRangeMax << "],\n"
        << "  \"physicalSubCameraIds\": [";
    for (size_t i = 0; i < physicalSubCameraIds.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << physicalSubCameraIds[i] << "\"";
    }
    oss << "],\n  \"rawStreamModes\": [\n";
    for (size_t i = 0; i < rawStreamModes.size(); ++i) {
        const auto& m = rawStreamModes[i];
        oss << "    {\"width\": " << m.width << ", \"height\": " << m.height
            << ", \"format\": " << static_cast<int>(m.format)
            << ", \"minFrameDurationNs\": " << m.minFrameDurationNs
            << ", \"maxRealFps\": " << m.maxRealFps() << "}";
        if (i + 1 < rawStreamModes.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n"
        << "  \"empiricalAudit\": {\n"
        << "    \"isTrueLinearRaw\": " << (empiricalAudit.isTrueLinearRaw ? "true" : "false") << ",\n"
        << "    \"hiddenSpatialDenoisingDetected\": "
        << (empiricalAudit.hiddenSpatialDenoisingDetected ? "true" : "false") << ",\n"
        << "    \"bakedLensShadingDetected\": "
        << (empiricalAudit.bakedLensShadingDetected ? "true" : "false") << ",\n"
        << "    \"effectiveActiveBitDepth\": " << empiricalAudit.effectiveActiveBitDepth << ",\n"
        << "    \"photonTransferLinearityR2\": " << empiricalAudit.photonTransferLinearityR2 << ",\n"
        << "    \"detectedPdafDefectCount\": " << empiricalAudit.detectedPdafDefectCount << ",\n"
        << "    \"diagnosticSummary\": \"" << empiricalAudit.diagnosticSummary << "\"\n"
        << "  }\n"
        << "}";
    return oss.str();
}

EmpiricalRawVerificationReport DeviceCapabilityProbe::verifyRawAuthenticityEmpirically(
    const uint16_t* rawFrameData,
    uint32_t width,
    uint32_t height,
    uint32_t rowStrideBytes,
    uint16_t blackLevel,
    uint16_t whiteLevel,
    const std::vector<IsoVariancePoint>& isoVarianceRamp) {
    EmpiricalRawVerificationReport report{};
    report.containerBitDepth = 16;
    report.rowStridePaddingPresent = (rowStrideBytes > width * sizeof(uint16_t));
    report.blackLevelStable = (blackLevel > 0 && blackLevel < whiteLevel / 4);

    // 1. Photon Transfer / ISO Noise Variance Linearity R^2 check
    if (isoVarianceRamp.size() >= 3) {
        const size_t n = isoVarianceRamp.size();
        double sumX = 0.0;
        double sumY = 0.0;
        for (const auto& pt : isoVarianceRamp) {
            const double x = static_cast<double>(pt.iso) * std::max(1.0, pt.meanSignalADU - blackLevel);
            sumX += x;
            sumY += pt.spatialVarianceADU2;
        }
        const double meanX = sumX / static_cast<double>(n);
        const double meanY = sumY / static_cast<double>(n);

        double covXY = 0.0;
        double varX = 0.0;
        double varY = 0.0;
        for (const auto& pt : isoVarianceRamp) {
            const double x = static_cast<double>(pt.iso) * std::max(1.0, pt.meanSignalADU - blackLevel);
            const double dx = x - meanX;
            const double dy = pt.spatialVarianceADU2 - meanY;
            covXY += dx * dy;
            varX += dx * dx;
            varY += dy * dy;
        }
        if (varX > 1e-9 && varY > 1e-9) {
            const double r = covXY / std::sqrt(varX * varY);
            report.photonTransferLinearityR2 = (covXY >= 0.0) ? (r * r) : 0.0;
        } else {
            report.photonTransferLinearityR2 = 0.0;
        }

        // If variance flattens or drops at high ISO (R^2 < 0.85), OEM is applying hidden NR on RAW_SENSOR
        report.hiddenSpatialDenoisingDetected = (report.photonTransferLinearityR2 < 0.85);
    }

    // 2. Inspect pixel buffer for effective active bit depth, flat-field vignetting, and PDAF defects
    if (rawFrameData != nullptr && width >= 16 && height >= 16) {
        const size_t strideShorts = rowStrideBytes / sizeof(uint16_t);
        uint16_t orMask = 0;
        uint16_t maxVal = 0;
        uint32_t pdafDefects = 0;

        for (uint32_t y = 2; y + 2 < height; ++y) {
            const uint16_t* row = rawFrameData + static_cast<size_t>(y) * strideShorts;
            const uint16_t* rowPrev2 = rawFrameData + static_cast<size_t>(y - 2) * strideShorts;
            const uint16_t* rowNext2 = rawFrameData + static_cast<size_t>(y + 2) * strideShorts;
            for (uint32_t x = 2; x + 2 < width; ++x) {
                const uint16_t val = row[x];
                orMask |= val;
                maxVal = std::max(maxVal, val);

                // Check same-CFA neighbor difference for isolated PDAF/dead pixel impulse
                const int32_t nMean = (static_cast<int32_t>(row[x - 2]) +
                                       static_cast<int32_t>(row[x + 2]) +
                                       static_cast<int32_t>(rowPrev2[x]) +
                                       static_cast<int32_t>(rowNext2[x])) / 4;
                const int32_t diff = std::abs(static_cast<int32_t>(val) - nMean);
                if (diff > static_cast<int32_t>(whiteLevel / 5)) {
                    ++pdafDefects;
                }
            }
        }
        report.detectedPdafDefectCount = pdafDefects;

        // Determine effective bit depth from highest bit and lowest toggled bits
        uint32_t highestBit = 10;
        if (whiteLevel > 16383 || maxVal > 16383) {
            highestBit = 16;
        } else if (whiteLevel > 4095 || maxVal > 4095) {
            highestBit = 14;
        } else if (whiteLevel > 1023 || maxVal > 1023) {
            highestBit = 12;
        }
        // Detect if lower bits are permanently zero (e.g. 10-bit left-shifted into 12/14/16-bit)
        uint32_t trailingZeroBits = 0;
        if (orMask != 0) {
            trailingZeroBits = static_cast<uint32_t>(__builtin_ctz(orMask));
        }
        report.effectiveActiveBitDepth =
            (highestBit > trailingZeroBits) ? (highestBit - trailingZeroBits) : highestBit;

        // Compute corner-to-center ratio on green channel to detect baked lens shading
        double centerSum = 0.0;
        size_t centerCount = 0;
        const uint32_t cx0 = width * 3 / 8;
        const uint32_t cx1 = width * 5 / 8;
        const uint32_t cy0 = height * 3 / 8;
        const uint32_t cy1 = height * 5 / 8;
        for (uint32_t y = cy0; y < cy1; ++y) {
            const uint16_t* row = rawFrameData + static_cast<size_t>(y) * strideShorts;
            for (uint32_t x = cx0; x < cx1; ++x) {
                centerSum += std::max<int32_t>(0, static_cast<int32_t>(row[x]) - blackLevel);
                ++centerCount;
            }
        }

        double cornerSum = 0.0;
        size_t cornerCount = 0;
        const uint32_t cw = std::max<uint32_t>(4, width / 8);
        const uint32_t ch = std::max<uint32_t>(4, height / 8);
        for (uint32_t y = 0; y < ch; ++y) {
            const uint16_t* row = rawFrameData + static_cast<size_t>(y) * strideShorts;
            for (uint32_t x = 0; x < cw; ++x) {
                cornerSum += std::max<int32_t>(0, static_cast<int32_t>(row[x]) - blackLevel);
                ++cornerCount;
            }
        }

        const double centerMean = (centerCount > 0) ? (centerSum / static_cast<double>(centerCount)) : 1.0;
        const double cornerMean = (cornerCount > 0) ? (cornerSum / static_cast<double>(cornerCount)) : 1.0;
        report.cornerToCenterFlatFieldRatio =
            (centerMean > 1.0) ? (cornerMean / centerMean) : 1.0;
        // Real optics have natural cos^4 falloff (typically 0.45..0.88). If ratio > 0.985 on flat field,
        // lens shading correction was baked in by the HAL.
        report.bakedLensShadingDetected = (report.cornerToCenterFlatFieldRatio > 0.985);
    }

    report.isTrueLinearRaw = !report.hiddenSpatialDenoisingDetected && report.blackLevelStable;
    std::ostringstream summary;
    summary << (report.isTrueLinearRaw ? "VERIFIED TRUE LINEAR RAW" : "WARNING: HAL MODIFIED RAW")
            << " | ActiveBits=" << report.effectiveActiveBitDepth
            << " | PTC_R2=" << report.photonTransferLinearityR2
            << " | PDAF_Defects=" << report.detectedPdafDefectCount;
    report.diagnosticSummary = summary.str();
    return report;
}

bool DeviceCapabilityProbe::mapHalCfaToPattern(uint8_t halCfaByte,
                                               l0::CfaPattern& outPattern) noexcept {
    switch (halCfaByte) {
        case 0:
            outPattern = l0::CfaPattern::RGGB;
            return true;
        case 1:
            outPattern = l0::CfaPattern::GRBG;
            return true;
        case 2:
            outPattern = l0::CfaPattern::GBRG;
            return true;
        case 3:
            outPattern = l0::CfaPattern::BGGR;
            return true;
        default:
            // 4 = RGB, 5 = MONO, 6 = NIR -> Not standard 2x2 Bayer CFA
            return false;
    }
}

std::vector<CameraDeviceCapabilityReport> DeviceCapabilityProbe::probeAllCameras(
    const std::string& hostSimulatedModel,
    bool allowHostSimulationOnEmpty) const {
    std::vector<CameraDeviceCapabilityReport> reports;

#if defined(__ANDROID__)
    (void)hostSimulatedModel;
    (void)allowHostSimulationOnEmpty;
    ACameraManager* mgr = ACameraManager_create();
    if (mgr != nullptr) {
        ACameraIdList* idList = nullptr;
        if (ACameraManager_getCameraIdList(mgr, &idList) == ACAMERA_OK && idList != nullptr) {
            for (int i = 0; i < idList->numCameras; ++i) {
                const char* camId = idList->cameraIds[i];
                ACameraMetadata* chars = nullptr;
                if (ACameraManager_getCameraCharacteristics(mgr, camId, &chars) == ACAMERA_OK &&
                    chars != nullptr) {
                    CameraDeviceCapabilityReport rep{};
                    rep.logicalCameraId = camId;
                    rep.isSimulatedReport = false;

                    ACameraMetadata_const_entry capEntry{};
                    if (ACameraMetadata_getConstEntry(
                            chars, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, &capEntry) == ACAMERA_OK) {
                        for (uint32_t c = 0; c < capEntry.count; ++c) {
                            const uint8_t cap = capEntry.data.u8[c];
                            if (cap == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_RAW) {
                                rep.supportsRawCapability = true;
                            } else if (cap == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR) {
                                rep.supportsManualSensor = true;
                            } else if (cap == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING) {
                                rep.supportsManualPostProcessing = true;
                            } else if (cap == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) {
                                rep.supportsLogicalMultiCamera = true;
                            }
                        }
                    }

                    ACameraMetadata_const_entry cfaEntry{};
                    if (ACameraMetadata_getConstEntry(
                            chars, ACAMERA_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &cfaEntry) == ACAMERA_OK &&
                        cfaEntry.count > 0) {
                        rep.cfaSupported = mapHalCfaToPattern(cfaEntry.data.u8[0], rep.cfaPattern);
                        if (!rep.cfaSupported) {
                            rep.supportsRawCapability = false;
                        }
                    }

                    ACameraMetadata_const_entry wlEntry{};
                    if (ACameraMetadata_getConstEntry(
                            chars, ACAMERA_SENSOR_INFO_WHITE_LEVEL, &wlEntry) == ACAMERA_OK &&
                        wlEntry.count > 0) {
                        rep.sensorWhiteLevel = static_cast<uint32_t>(wlEntry.data.i32[0]);
                    }

                    ACameraMetadata_const_entry blEntry{};
                    if (ACameraMetadata_getConstEntry(
                            chars, ACAMERA_SENSOR_BLACK_LEVEL_PATTERN, &blEntry) == ACAMERA_OK &&
                        blEntry.count >= 4) {
                        for (int b = 0; b < 4; ++b) {
                            rep.staticBlackLevelRggb[b] = static_cast<uint16_t>(blEntry.data.i32[b]);
                        }
                    }

                    ACameraMetadata_const_entry tsEntry{};
                    if (ACameraMetadata_getConstEntry(
                            chars, ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE, &tsEntry) == ACAMERA_OK &&
                        tsEntry.count > 0) {
                        rep.timestampSourceRealtime =
                            (tsEntry.data.u8[0] == ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME);
                    }

                    // Enumerate RAW stream configurations & minFrameDurations
                    ACameraMetadata_const_entry streamEntry{};
                    ACameraMetadata_const_entry minDurEntry{};
                    const bool hasMinDur =
                        (ACameraMetadata_getConstEntry(
                             chars, ACAMERA_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, &minDurEntry) ==
                         ACAMERA_OK);

                    if (ACameraMetadata_getConstEntry(
                            chars, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &streamEntry) ==
                        ACAMERA_OK) {
                        for (uint32_t s = 0; s + 3 < streamEntry.count; s += 4) {
                            const int32_t fmt = streamEntry.data.i32[s];
                            const int32_t w = streamEntry.data.i32[s + 1];
                            const int32_t h = streamEntry.data.i32[s + 2];
                            const int32_t isInput = streamEntry.data.i32[s + 3];
                            if (isInput != 0) continue;

                            l0::PixelFormat pixFmt = l0::PixelFormat::UNKNOWN;
                            if (fmt == AIMAGE_FORMAT_RAW16) {
                                pixFmt = l0::PixelFormat::RAW16_SENSOR_LE;
                            } else if (fmt == AIMAGE_FORMAT_RAW10) {
                                pixFmt = l0::PixelFormat::RAW10_MIPI_PACKED;
                            } else if (fmt == AIMAGE_FORMAT_RAW12) {
                                pixFmt = l0::PixelFormat::RAW12_MIPI_PACKED;
                            }
                            if (pixFmt != l0::PixelFormat::UNKNOWN) {
                                int64_t minDurNs = 33'333'333LL;
                                if (hasMinDur) {
                                    for (uint32_t d = 0; d + 3 < minDurEntry.count; d += 4) {
                                        if (minDurEntry.data.i64[d] == fmt &&
                                            minDurEntry.data.i64[d + 1] == w &&
                                            minDurEntry.data.i64[d + 2] == h) {
                                            minDurNs = minDurEntry.data.i64[d + 3];
                                            break;
                                        }
                                    }
                                }
                                rep.rawStreamModes.push_back(
                                    {static_cast<uint32_t>(w), static_cast<uint32_t>(h), pixFmt, minDurNs, 0});
                            }
                        }
                    }

                    // Note: Standalone AImageReader_newWithUsage checks gralloc allocation only;
                    // camera HAL session support must still be verified when creating ACameraCaptureSession.
                    AImageReader* testReader = nullptr;
                    if (AImageReader_newWithUsage(
                            640,
                            480,
                            AIMAGE_FORMAT_RAW16,
                            AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                            2,
                            &testReader) == AMEDIA_OK &&
                        testReader != nullptr) {
                        rep.gpuHardwareBufferRawImportSupported = true;
                        rep.cpuMapFallbackRequired = true; // Keep CPU fallback armed until session succeeds
                        AImageReader_delete(testReader);
                    } else {
                        rep.gpuHardwareBufferRawImportSupported = false;
                        rep.cpuMapFallbackRequired = true;
                    }

                    rep.empiricalAudit.isTrueLinearRaw = false;
                    rep.empiricalAudit.diagnosticSummary =
                        "AWAITING LIVE FRAME CAPTURE FOR PTC/VIGNETTE AUDIT";

                    ACameraMetadata_free(chars);
                    reports.push_back(std::move(rep));
                }
            }
            ACameraManager_deleteCameraIdList(idList);
        }
        ACameraManager_delete(mgr);
    }
#else
    if (reports.empty() && allowHostSimulationOnEmpty) {
        // Non-Android host CI simulation only (explicitly marked isSimulatedReport = true)
        CameraDeviceCapabilityReport mainCam{};
        mainCam.logicalCameraId = "0";
        mainCam.physicalSubCameraIds = {"0", "2", "3"};
        mainCam.sensorModelTag = hostSimulatedModel + " [HOST_SIMULATION]";
        mainCam.isSimulatedReport = true;
        mainCam.supportsRawCapability = true;
        mainCam.supportsManualSensor = true;
        mainCam.supportsManualPostProcessing = true;
        mainCam.supportsLogicalMultiCamera = true;
        mainCam.supportsReadoutTimestamp = true;
        mainCam.timestampSourceRealtime = true;
        mainCam.gpuHardwareBufferRawImportSupported = false;
        mainCam.cpuMapFallbackRequired = true;
        mainCam.cfaSupported = true;
        mainCam.cfaPattern = l0::CfaPattern::RGGB;
        mainCam.sensorWhiteLevel = 4095;
        mainCam.staticBlackLevelRggb[0] = 256;
        mainCam.staticBlackLevelRggb[1] = 256;
        mainCam.staticBlackLevelRggb[2] = 256;
        mainCam.staticBlackLevelRggb[3] = 256;
        mainCam.isoRangeMin = 50;
        mainCam.isoRangeMax = 12800;
        mainCam.rawStreamModes = {
            {4096, 3072, l0::PixelFormat::RAW16_SENSOR_LE, 33'333'333LL, 0},
            {4096, 2160, l0::PixelFormat::RAW12_MIPI_PACKED, 16'666'666LL, 0},
            {3840, 2160, l0::PixelFormat::RAW10_MIPI_PACKED, 16'666'666LL, 0},
            {1920, 1080, l0::PixelFormat::RAW12_MIPI_PACKED, 8'333'333LL, 0}
        };
        mainCam.empiricalAudit.isTrueLinearRaw = false;
        mainCam.empiricalAudit.effectiveActiveBitDepth = 12;
        mainCam.empiricalAudit.photonTransferLinearityR2 = 0.0;
        mainCam.empiricalAudit.diagnosticSummary =
            "SIMULATED HOST REPORT (NOT HARDWARE VERIFIED)";
        reports.push_back(std::move(mainCam));

        CameraDeviceCapabilityReport uwCam{};
        uwCam.logicalCameraId = "2";
        uwCam.sensorModelTag = hostSimulatedModel + " Ultrawide [HOST_SIMULATION]";
        uwCam.isSimulatedReport = true;
        uwCam.supportsRawCapability = true;
        uwCam.supportsManualSensor = true;
        uwCam.supportsManualPostProcessing = true;
        uwCam.cfaSupported = true;
        uwCam.cfaPattern = l0::CfaPattern::RGGB;
        uwCam.sensorWhiteLevel = 1023;
        uwCam.staticBlackLevelRggb[0] = 64;
        uwCam.staticBlackLevelRggb[1] = 64;
        uwCam.staticBlackLevelRggb[2] = 64;
        uwCam.staticBlackLevelRggb[3] = 64;
        uwCam.rawStreamModes = {
            {3264, 2448, l0::PixelFormat::RAW10_MIPI_PACKED, 33'333'333LL, 0}
        };
        uwCam.empiricalAudit.isTrueLinearRaw = false;
        uwCam.empiricalAudit.effectiveActiveBitDepth = 10;
        uwCam.empiricalAudit.photonTransferLinearityR2 = 0.0;
        uwCam.empiricalAudit.diagnosticSummary =
            "SIMULATED HOST REPORT (NOT HARDWARE VERIFIED)";
        reports.push_back(std::move(uwCam));
    }
#endif

    return reports;
}

} // namespace apexcine::lp
