#pragma once

#include "apexcine/l0_runtime/ArenaAllocator.hpp"
#include "apexcine/l0_runtime/BufferContract.hpp"
#include "apexcine/l0_runtime/FrameClockAndTelemetry.hpp"
#include "apexcine/l0_runtime/SpscRingBuffer.hpp"
#include "apexcine/lp_platform/DeviceQuirkDatabase.hpp"
#include <atomic>
#include <cstdint>
#include <string>

namespace apexcine::l1 {

/**
 * Complete ISP-Bypass Capture Request Contract.
 * Every ISP post-processing block is explicitly locked OFF.
 */
struct ManualCaptureRequestConfig {
    std::string logicalCameraId{"0"};
    std::string physicalSubCameraId{""}; // Non-empty routes via ACaptureSessionOutput_createWithPhysicalCamera
    uint32_t width{4096};
    uint32_t height{2160};
    l0::PixelFormat format{l0::PixelFormat::RAW12_MIPI_PACKED};
    int64_t exposureTimeNs{10'416'666LL}; // 180-deg shutter @ 48fps or 1/96s
    int64_t frameDurationNs{41'666'666LL}; // 24.000 fps locked
    int32_t isoSensitivity{400};
    float focusDistanceDiopters{0.8f};
    bool manualFocusEnabled{true};

    // Mandatory ISP bypass locks
    bool aeModeOff{true};
    bool awbModeOff{true};
    bool noiseReductionOff{true};
    bool edgeEnhancementOff{true};
    bool lensShadingCorrectionOff{true};
    bool requestLensShadingMetadataMap{true};
    bool colorAberrationOff{true};
    bool hotPixelMapOff{true};
    bool blackLevelLockOn{true};
    bool opticalStabilizationOn{true};

    constexpr bool verifyPureRawContract() const noexcept {
        return aeModeOff && awbModeOff && noiseReductionOff && edgeEnhancementOff &&
               lensShadingCorrectionOff && colorAberrationOff && blackLevelLockOn &&
               exposureTimeNs > 0 && frameDurationNs >= exposureTimeNs && isoSensitivity > 0;
    }
};

struct ImuSample {
    int64_t timestampBoottimeNs{0};
    float gyroRadSec[3]{0.0f, 0.0f, 0.0f};
    float accelMeterSec2[3]{0.0f, 0.0f, 9.81f};
};

struct PerFrameCaptureMetadata {
    uint64_t frameNumber{0};
    int64_t sensorExposureStartNs{0};
    int64_t sensorReadoutStartNs{0};
    int64_t exposureTimeNs{0};
    int64_t frameDurationNs{0};
    int64_t rollingShutterDurationNs{11'200'000LL};
    int32_t isoSensitivity{400};
    float lensFocusDistanceDiopters{0.0f};
    float dynamicBlackLevelRggb[4]{256.0f, 256.0f, 256.0f, 256.0f};
    uint32_t whiteLevel{4095};
    float asShotNeutralRggb[3]{0.48f, 1.0f, 0.62f};
    float greenSplit{0.0f};
    uint32_t lensShadingMapWidth{17};
    uint32_t lensShadingMapHeight{13};
    float lensShadingCornerGain{1.42f};
    ImuSample interpolatedMidExposureImu{};
};

/**
 * Layer 1: Native NDK Camera2 Capture Engine & IMU Synchronizer.
 *
 * - Validates the strict ISP-Bypass request contract (all OEM NR/Edge/Shading/Tonemap OFF,
 *   BLACK_LEVEL_LOCK ON).
 * - Ingests either CPU-mapped AImageReader planes (copied into locked L0 Arena slabs) or
 *   zero-copy AHardwareBuffer handles (descriptor-only registration without memcpy).
 * - Synchronizes lock-free IMU samples to each frame's mid-exposure timestamp:
 *   t_mid = SENSOR_TIMESTAMP + EXPOSURE_TIME/2 + rollingShutterReadoutNs/2.
 * - Thread-safety: pushImuSample() is called by the IMU sensor thread (producer);
 *   onRawFrameAvailable() / findClosestImuSample() are called by the frame ingest thread.
 */
class NdkCameraCaptureEngine {
public:
    NdkCameraCaptureEngine(l0::ArenaAllocator& arena,
                           l0::FrameClockAndTelemetry& telemetry,
                           const lp::DeviceProfileRecord& deviceProfile) noexcept;
    ~NdkCameraCaptureEngine() noexcept;

    bool configureSession(const ManualCaptureRequestConfig& config) noexcept;

    bool startCapture() noexcept;
    void stopCapture() noexcept;

    bool pushImuSample(const ImuSample& sample) noexcept;

    /**
     * Ingests a CPU-mapped raw sensor frame into an L0 Arena slab and SPSC Ring Buffer
     * with zero dynamic heap allocations.
     */
    bool onRawFrameAvailable(const uint8_t* planeData,
                             size_t planeBytes,
                             uint32_t rowStrideBytes,
                             uint32_t pixelStrideBytes,
                             const PerFrameCaptureMetadata& metadata) noexcept;

    /**
     * Zero-copy descriptor ingestion path for AHardwareBuffer-backed frames (no payload memcpy).
     */
    bool onHardwareBufferFrameAvailable(size_t framePayloadBytes,
                                        uint32_t rowStrideBytes,
                                        uint32_t pixelStrideBytes,
                                        const PerFrameCaptureMetadata& metadata) noexcept;

    bool popCapturedFrame(l0::BufferDescriptor& outDesc,
                          PerFrameCaptureMetadata& outMeta) noexcept;

    bool isCapturing() const noexcept {
        return isCapturing_.load(std::memory_order_acquire);
    }

    bool usingHardwareBufferZeroCopy() const noexcept {
        return usingAhbZeroCopy_;
    }

    const ManualCaptureRequestConfig& activeConfig() const noexcept {
        return activeConfig_;
    }

private:
    ImuSample findClosestImuSample(int64_t targetTimestampNs) noexcept;

    l0::ArenaAllocator& arena_;
    l0::FrameClockAndTelemetry& telemetry_;
    lp::DeviceProfileRecord deviceProfile_;
    ManualCaptureRequestConfig activeConfig_{};
    bool isConfigured_{false};
    bool usingAhbZeroCopy_{false};
    std::atomic<bool> isCapturing_{false};
    std::atomic<uint64_t> sequenceCounter_{0};

    struct FrameEnvelope {
        l0::BufferDescriptor descriptor{};
        PerFrameCaptureMetadata metadata{};
    };

    l0::SpscRingBuffer<FrameEnvelope, 64> captureRing_{};
    l0::SpscRingBuffer<ImuSample, 512> imuRing_{};
    // Strictly owned by the consumer thread (findClosestImuSample); never touched by pushImuSample
    ImuSample consumerLatestImu_{};
    bool hasConsumerImu_{false};
};

} // namespace apexcine::l1
