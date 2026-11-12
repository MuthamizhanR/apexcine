#pragma once

#include "apexcine/l0_runtime/BufferContract.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace apexcine::lp {

struct RawStreamMode {
    uint32_t width{0};
    uint32_t height{0};
    l0::PixelFormat format{l0::PixelFormat::RAW10_MIPI_PACKED};
    int64_t minFrameDurationNs{33'333'333LL};
    int64_t stallDurationNs{0};

    constexpr double maxRealFps() const noexcept {
        return (minFrameDurationNs > 0) ? (1e9 / static_cast<double>(minFrameDurationNs)) : 0.0;
    }

    constexpr double uncompressedBandwidthMBps(double targetFps) const noexcept {
        double bytesPerPixel = 2.0;
        if (format == l0::PixelFormat::RAW10_MIPI_PACKED) {
            bytesPerPixel = 1.25;
        } else if (format == l0::PixelFormat::RAW12_MIPI_PACKED) {
            bytesPerPixel = 1.5;
        }
        return (static_cast<double>(width) * static_cast<double>(height) * bytesPerPixel * targetFps) /
               (1024.0 * 1024.0);
    }
};

struct EmpiricalRawVerificationReport {
    bool isTrueLinearRaw{true};
    bool hiddenSpatialDenoisingDetected{false};
    bool bakedLensShadingDetected{false};
    bool blackLevelStable{true};
    bool rowStridePaddingPresent{false};
    uint32_t containerBitDepth{16};
    uint32_t effectiveActiveBitDepth{12};
    double photonTransferLinearityR2{0.995}; // >= 0.96 indicates genuine un-denoised linear sensor RAW
    double cornerToCenterFlatFieldRatio{0.68}; // < 0.92 on raw optical flat-field indicates un-baked shading
    uint32_t detectedPdafDefectCount{0};
    std::string diagnosticSummary{};
};

struct CameraDeviceCapabilityReport {
    std::string logicalCameraId{"0"};
    std::vector<std::string> physicalSubCameraIds{};
    std::string sensorModelTag{"Unknown_CMOS"};
    bool isSimulatedReport{false};
    bool supportsRawCapability{false};
    bool supportsManualSensor{false};
    bool supportsManualPostProcessing{false};
    bool supportsLogicalMultiCamera{false};
    bool supportsReadoutTimestamp{false}; // API 33+ SENSOR_READOUT_TIMESTAMP
    bool timestampSourceRealtime{true};   // SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME
    bool gpuHardwareBufferRawImportSupported{false}; // Standalone AImageReader check; requires session verification
    bool cpuMapFallbackRequired{true};
    bool cfaSupported{true}; // False for MONO (5), NIR (6), or non-Bayer RGB (4)
    l0::CfaPattern cfaPattern{l0::CfaPattern::RGGB};
    uint32_t sensorWhiteLevel{4095};
    uint16_t staticBlackLevelRggb[4]{64, 64, 64, 64};
    int32_t isoRangeMin{50};
    int32_t isoRangeMax{6400};
    int64_t exposureTimeMinNs{100'000LL};
    int64_t exposureTimeMaxNs{1'000'000'000LL};
    float minimumFocusDistanceDiopters{10.0f};
    uint8_t referenceIlluminant1{17}; // Standard Light A (2856K)
    uint8_t referenceIlluminant2{21}; // D65 (6504K)
    float colorMatrix1[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
    float colorMatrix2[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
    float forwardMatrix1[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
    float forwardMatrix2[9]{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::vector<RawStreamMode> rawStreamModes{};
    EmpiricalRawVerificationReport empiricalAudit{};

    std::string toJson() const;
};

struct IsoVariancePoint {
    uint32_t iso{100};
    double meanSignalADU{0.0};
    double spatialVarianceADU2{0.0};
};

class DeviceCapabilityProbe {
public:
    DeviceCapabilityProbe() = default;
    ~DeviceCapabilityProbe() = default;

    /**
     * Validates an Android HAL CFA byte (ACAMERA_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT).
     * Returns true only for standard 2x2 Bayer patterns (0=RGGB, 1=GRBG, 2=GBRG, 3=BGGR).
     * Rejects MONO (5), RGB (4), NIR (6), or unknown values instead of bitmasking.
     */
    static bool mapHalCfaToPattern(uint8_t halCfaByte, l0::CfaPattern& outPattern) noexcept;

    /**
     * Probes all cameras via NDK ACameraManager on Android.
     * On non-Android host builds (or when allowHostSimulationOnEmpty is explicitly true in tests),
     * returns a clearly labeled simulated report. Never fabricates hardware capabilities on Android.
     */
    std::vector<CameraDeviceCapabilityReport> probeAllCameras(
        const std::string& hostSimulatedModel = "OnePlus Nord 5",
        bool allowHostSimulationOnEmpty = true) const;

    /**
     * Empirically verifies whether a captured RAW16 frame + ISO noise ramp is genuinely raw.
     */
    static EmpiricalRawVerificationReport verifyRawAuthenticityEmpirically(
        const uint16_t* rawFrameData,
        uint32_t width,
        uint32_t height,
        uint32_t rowStrideBytes,
        uint16_t blackLevel,
        uint16_t whiteLevel,
        const std::vector<IsoVariancePoint>& isoVarianceRamp);
};

} // namespace apexcine::lp
