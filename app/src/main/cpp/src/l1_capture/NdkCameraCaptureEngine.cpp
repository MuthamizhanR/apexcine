#include "apexcine/l1_capture/NdkCameraCaptureEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(__ANDROID__)
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <media/NdkImageReader.h>
#endif

namespace apexcine::l1 {

NdkCameraCaptureEngine::NdkCameraCaptureEngine(l0::ArenaAllocator& arena,
                                               l0::FrameClockAndTelemetry& telemetry,
                                               const lp::DeviceProfileRecord& deviceProfile) noexcept
    : arena_(arena), telemetry_(telemetry), deviceProfile_(deviceProfile) {}

NdkCameraCaptureEngine::~NdkCameraCaptureEngine() noexcept {
    stopCapture();
}

bool NdkCameraCaptureEngine::configureSession(const ManualCaptureRequestConfig& config) noexcept {
    if (!config.verifyPureRawContract()) {
        return false;
    }
    activeConfig_ = config;
    // Respect device quirk DB for AHardwareBuffer zero-copy vs safe CPU plane map
    usingAhbZeroCopy_ = !deviceProfile_.quirkForceCpuImageReaderMap;
    isConfigured_ = true;
    return true;
}

bool NdkCameraCaptureEngine::startCapture() noexcept {
    if (!isConfigured_) {
        return false;
    }
    isCapturing_.store(true, std::memory_order_release);
    return true;
}

void NdkCameraCaptureEngine::stopCapture() noexcept {
    isCapturing_.store(false, std::memory_order_release);
    FrameEnvelope env{};
    while (captureRing_.tryPop(env)) {
        if (env.descriptor.slabIndex != 0xFFFFFFFFu) {
            (void)arena_.releaseSlab(env.descriptor.slabIndex);
        }
    }
}

bool NdkCameraCaptureEngine::pushImuSample(const ImuSample& sample) noexcept {
    // Producer only writes to the lock-free SPSC ring; never touches consumer state.
    return imuRing_.tryPush(sample);
}

ImuSample NdkCameraCaptureEngine::findClosestImuSample(int64_t targetTimestampNs) noexcept {
    ImuSample best = consumerLatestImu_;
    int64_t bestDiff = hasConsumerImu_
                           ? std::abs(best.timestampBoottimeNs - targetTimestampNs)
                           : INT64_MAX;
    ImuSample candidate{};
    while (imuRing_.tryPop(candidate)) {
        const int64_t diff = std::abs(candidate.timestampBoottimeNs - targetTimestampNs);
        if (diff <= bestDiff) {
            best = candidate;
            bestDiff = diff;
        }
        consumerLatestImu_ = candidate;
        hasConsumerImu_ = true;
    }
    return best;
}

bool NdkCameraCaptureEngine::onRawFrameAvailable(
    const uint8_t* planeData,
    size_t planeBytes,
    uint32_t rowStrideBytes,
    uint32_t pixelStrideBytes,
    const PerFrameCaptureMetadata& metadata) noexcept {
    if (!isCapturing_.load(std::memory_order_acquire) || planeData == nullptr || planeBytes == 0) {
        return false;
    }

    const int64_t startNs = l0::FrameClockAndTelemetry::nowMonotonicNs();
    l0::BufferDescriptor desc{};
    uint8_t* slabPtr = nullptr;

    if (!arena_.acquireSlab(l0::ProducerStage::L1_NDK_IMAGE_READER,
                            l0::ConsumerStage::L2_NEON_UNPACKER,
                            desc,
                            slabPtr)) {
        telemetry_.recordDroppedFrame();
        return false;
    }

    if (planeBytes > desc.capacityBytes) {
        (void)arena_.releaseSlab(desc.slabIndex);
        telemetry_.recordDroppedFrame();
        return false;
    }

    std::memcpy(slabPtr, planeData, planeBytes);

    const uint64_t seq = sequenceCounter_.fetch_add(1, std::memory_order_relaxed) + 1;
    desc.frameSequenceId = seq;
    desc.sensorTimestampNs = metadata.sensorExposureStartNs;
    desc.readoutTimestampNs = metadata.sensorReadoutStartNs;
    desc.exposureDurationNs = metadata.exposureTimeNs;
    desc.rollingShutterReadoutNs = metadata.rollingShutterDurationNs;
    desc.isoSensitivity = static_cast<uint32_t>(metadata.isoSensitivity);
    desc.width = activeConfig_.width;
    desc.height = activeConfig_.height;
    desc.rowStrideBytes = rowStrideBytes;
    desc.pixelStrideBytes = pixelStrideBytes;
    desc.bitDepth = deviceProfile_.activeBitDepth;
    for (int i = 0; i < 4; ++i) {
        desc.blackLevel[i] = static_cast<uint16_t>(
            std::clamp(metadata.dynamicBlackLevelRggb[i], 0.0f, 65535.0f));
    }
    desc.whiteLevel = static_cast<uint16_t>(std::min<uint32_t>(metadata.whiteLevel, 65535u));
    desc.greenSplit = metadata.greenSplit;
    desc.format = activeConfig_.format;
    desc.cfa = deviceProfile_.cfaPattern;
    desc.residency = l0::MemoryResidency::CPU_LOCKED_ARENA;
    desc.validBytes = planeBytes;

    PerFrameCaptureMetadata enrichedMeta = metadata;
    enrichedMeta.frameNumber = seq;
    const int64_t midExpNs = desc.midExposureTimestampNs();
    enrichedMeta.interpolatedMidExposureImu = findClosestImuSample(midExpNs);

    FrameEnvelope env{desc, enrichedMeta};
    if (!captureRing_.tryPush(env)) {
        (void)arena_.releaseSlab(desc.slabIndex);
        telemetry_.recordDroppedFrame();
        return false;
    }

    const int64_t endNs = l0::FrameClockAndTelemetry::nowMonotonicNs();
    telemetry_.recordSpan("L1_NDK_FrameIngest_CpuCopy", startNs, endNs, 1, seq);
    return true;
}

bool NdkCameraCaptureEngine::onHardwareBufferFrameAvailable(
    size_t framePayloadBytes,
    uint32_t rowStrideBytes,
    uint32_t pixelStrideBytes,
    const PerFrameCaptureMetadata& metadata) noexcept {
    if (!isCapturing_.load(std::memory_order_acquire) || framePayloadBytes == 0) {
        return false;
    }

    const int64_t startNs = l0::FrameClockAndTelemetry::nowMonotonicNs();
    const uint64_t seq = sequenceCounter_.fetch_add(1, std::memory_order_relaxed) + 1;

    l0::BufferDescriptor desc{};
    desc.frameSequenceId = seq;
    desc.sensorTimestampNs = metadata.sensorExposureStartNs;
    desc.readoutTimestampNs = metadata.sensorReadoutStartNs;
    desc.exposureDurationNs = metadata.exposureTimeNs;
    desc.rollingShutterReadoutNs = metadata.rollingShutterDurationNs;
    desc.isoSensitivity = static_cast<uint32_t>(metadata.isoSensitivity);
    desc.width = activeConfig_.width;
    desc.height = activeConfig_.height;
    desc.rowStrideBytes = rowStrideBytes;
    desc.pixelStrideBytes = pixelStrideBytes;
    desc.bitDepth = deviceProfile_.activeBitDepth;
    for (int i = 0; i < 4; ++i) {
        desc.blackLevel[i] = static_cast<uint16_t>(
            std::clamp(metadata.dynamicBlackLevelRggb[i], 0.0f, 65535.0f));
    }
    desc.whiteLevel = static_cast<uint16_t>(std::min<uint32_t>(metadata.whiteLevel, 65535u));
    desc.greenSplit = metadata.greenSplit;
    desc.format = activeConfig_.format;
    desc.cfa = deviceProfile_.cfaPattern;
    desc.residency = l0::MemoryResidency::AHARDWAREBUFFER_IMPORT;
    desc.producer = l0::ProducerStage::L1_NDK_IMAGE_READER;
    desc.consumer = l0::ConsumerStage::L2_VULKAN_COMPUTE_PREVIEW;
    desc.slabIndex = 0xFFFFFFFFu;
    desc.capacityBytes = framePayloadBytes;
    desc.validBytes = framePayloadBytes;

    PerFrameCaptureMetadata enrichedMeta = metadata;
    enrichedMeta.frameNumber = seq;
    const int64_t midExpNs = desc.midExposureTimestampNs();
    enrichedMeta.interpolatedMidExposureImu = findClosestImuSample(midExpNs);

    FrameEnvelope env{desc, enrichedMeta};
    if (!captureRing_.tryPush(env)) {
        telemetry_.recordDroppedFrame();
        return false;
    }

    const int64_t endNs = l0::FrameClockAndTelemetry::nowMonotonicNs();
    telemetry_.recordSpan("L1_NDK_FrameIngest_ZeroCopyAhb", startNs, endNs, 1, seq);
    return true;
}

bool NdkCameraCaptureEngine::popCapturedFrame(l0::BufferDescriptor& outDesc,
                                              PerFrameCaptureMetadata& outMeta) noexcept {
    FrameEnvelope env{};
    if (!captureRing_.tryPop(env)) {
        return false;
    }
    outDesc = env.descriptor;
    outMeta = env.metadata;
    return true;
}

} // namespace apexcine::l1
