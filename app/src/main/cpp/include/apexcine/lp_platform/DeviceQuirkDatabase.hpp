#pragma once

#include "apexcine/l0_runtime/BufferContract.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace apexcine::lp {

struct DualIlluminantColorProfile {
    uint32_t illuminant1Kelvin{2856}; // CIE StdA
    uint32_t illuminant2Kelvin{6504}; // CIE D65
    // Default placeholder: sRGB-to-XYZ(D50) reference matrix until per-sensor spectral/chart calibration is loaded
    float forwardMatrix1_XYZ_D50[9]{
        0.4360747f, 0.3850649f, 0.1430804f,
        0.2225045f, 0.7168786f, 0.0606169f,
        0.0139322f, 0.0971045f, 0.7141733f};
    float forwardMatrix2_XYZ_D50[9]{
        0.4360747f, 0.3850649f, 0.1430804f,
        0.2225045f, 0.7168786f, 0.0606169f,
        0.0139322f, 0.0971045f, 0.7141733f};
    bool isColorimetricallyCalibrated{false};
    float measuredMeanDeltaE2000_D50{-1.0f};  // < 0 indicates unmeasured placeholder
    float measuredMeanDeltaE2000_StdA{-1.0f}; // < 0 indicates unmeasured placeholder
};

struct DeviceProfileRecord {
    std::string profileId{"generic_raw_v1"};
    std::string manufacturer{"Generic"};
    std::string modelPattern{"*"};
    std::string marketingName{"Generic Android RAW Camera"};
    std::string socName{"Generic ARMv8-A"};
    std::string primarySensorName{"Generic CMOS"};
    std::string storageTier{"UFS 3.1"};
    bool isHardwareVerified{false}; // False until validated on physical hardware + empirical RAW audit

    // Sensor readout & bit depth fallback parameters (overridden at runtime by CameraCharacteristics)
    l0::CfaPattern cfaPattern{l0::CfaPattern::RGGB};
    uint32_t activeBitDepth{12};
    uint16_t blackLevelRggb[4]{256, 256, 256, 256};
    uint16_t whiteLevel{4095};
    uint32_t rowStrideAlignmentBytes{64};
    float rollingShutterReadout4kMs{11.5f};
    float rollingShutterReadout1080pMs{6.2f};

    // Dual-Conversion-Gain (DCG) & Exposure Intelligence
    uint32_t baseNativeIso{100};
    uint32_t dcgJumpIso{400}; // Nominal High Conversion Gain kick-in ISO (requires PTC verification per device)
    float maxDynamicRangeStops{13.6f};

    // Quirk flags (data-driven, zero code branching by model name)
    bool quirkForceCpuImageReaderMap{false};
    bool quirkRowStridePaddingOnRaw10{false};
    bool quirkPdafRowDefectFilterRequired{false};
    bool quirkGreenSplitCorrectionRequired{true};
    bool quirkDisableODirectOnF2fs{false};
    uint32_t pdafDefectRowPeriod{16};
    uint32_t vulkanOptimalWorkgroupSizeX{16};
    uint32_t vulkanOptimalWorkgroupSizeY{16};
    uint32_t maxSustainedStorageMBps{380};

    // Physical camera IDs exposed under logical multi-camera
    std::vector<std::string> physicalCameraIds{"0", "2", "3"};

    // Dual-Illuminant Color Profile (ACES IDT)
    DualIlluminantColorProfile colorProfile{};
};

/**
 * Layer LP: Data-Driven Device Profile & Quirk Database.
 *
 * Loads machine-readable JSON profiles (bundled or OTA updated) and matches devices
 * by manufacturer/model/board tokens without hardcoded if-chains in the capture/ISP engine.
 */
class DeviceQuirkDatabase {
public:
    DeviceQuirkDatabase();
    ~DeviceQuirkDatabase() = default;

    /**
     * Parses a JSON array of device profile objects and merges/updates the database.
     */
    size_t loadProfilesFromJson(const std::string& jsonContent);

    /**
     *Serializes all loaded device profiles into a formatted JSON document.
     */
    std::string exportAllProfilesToJson() const;

    /**
     * Resolves the best-matching profile for a given manufacturer & model string.
     * Falls back to a safe synthesized default profile if no exact/pattern match is found.
     */
    DeviceProfileRecord matchDevice(const std::string& manufacturer,
                                    const std::string& model) const;

    const std::vector<DeviceProfileRecord>& allProfiles() const noexcept {
        return profiles_;
    }

private:
    void populateBuiltInVerifiedMatrix();
    std::vector<DeviceProfileRecord> profiles_{};
};

} // namespace apexcine::lp
