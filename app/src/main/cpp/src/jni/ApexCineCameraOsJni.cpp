#if defined(__ANDROID__)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <jni.h>
#include <sstream>
#include <string>

#include "apexcine/l0_runtime/ArenaAllocator.hpp"
#include "apexcine/l0_runtime/EdfScheduler.hpp"
#include "apexcine/l0_runtime/FrameClockAndTelemetry.hpp"
#include "apexcine/l1_capture/NdkCameraCaptureEngine.hpp"
#include "apexcine/l2_pipeline/ComputationalRawMerge.hpp"
#include "apexcine/l2_pipeline/LosslessBayerCodec.hpp"
#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"
#include "apexcine/l2_pipeline/SensorCalibrationAndClarity.hpp"
#include "apexcine/l3_container/ApexRawContainer.hpp"
#include "apexcine/l4_color/AcesColorPipeline.hpp"
#include "apexcine/lp_platform/DeviceCapabilityProbe.hpp"
#include "apexcine/lp_platform/DeviceQuirkDatabase.hpp"
#include "apexcine/lp_platform/StorageBenchmarkAndIoProber.hpp"
#include "apexcine/lp_platform/ThermalAndAdpfManager.hpp"

namespace {
apexcine::lp::DeviceQuirkDatabase g_quirkDb;
apexcine::lp::DeviceCapabilityProbe g_probe;
apexcine::lp::ThermalAndAdpfManager g_adpfMgr;
} // namespace

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_example_engine_NativeCameraOsBridge_nativeProbeDeviceCapabilitiesJson(
    JNIEnv* env,
    jobject /*thiz*/,
    jstring manufacturerJ,
    jstring modelJ) {
    const char* mfgChars = (manufacturerJ != nullptr) ? env->GetStringUTFChars(manufacturerJ, nullptr) : "OnePlus";
    const char* modelChars = (modelJ != nullptr) ? env->GetStringUTFChars(modelJ, nullptr) : "OnePlus Nord 5";
    std::string mfg(mfgChars ? mfgChars : "OnePlus");
    std::string model(modelChars ? modelChars : "OnePlus Nord 5");
    if (manufacturerJ != nullptr && mfgChars != nullptr) env->ReleaseStringUTFChars(manufacturerJ, mfgChars);
    if (modelJ != nullptr && modelChars != nullptr) env->ReleaseStringUTFChars(modelJ, modelChars);

    auto reports = g_probe.probeAllCameras(model);
    std::string json = "[\n";
    for (size_t i = 0; i < reports.size(); ++i) {
        json += reports[i].toJson();
        if (i + 1 < reports.size()) json += ",\n";
    }
    json += "\n]";
    return env->NewStringUTF(json.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_engine_NativeCameraOsBridge_nativeMatchDeviceProfileJson(
    JNIEnv* env,
    jobject /*thiz*/,
    jstring manufacturerJ,
    jstring modelJ) {
    const char* mfgChars = (manufacturerJ != nullptr) ? env->GetStringUTFChars(manufacturerJ, nullptr) : "Generic";
    const char* modelChars = (modelJ != nullptr) ? env->GetStringUTFChars(modelJ, nullptr) : "Unknown";
    std::string mfg(mfgChars ? mfgChars : "Generic");
    std::string model(modelChars ? modelChars : "Unknown");
    if (manufacturerJ != nullptr && mfgChars != nullptr) env->ReleaseStringUTFChars(manufacturerJ, mfgChars);
    if (modelJ != nullptr && modelChars != nullptr) env->ReleaseStringUTFChars(modelJ, modelChars);

    const auto p = g_quirkDb.matchDevice(mfg, model);
    std::ostringstream oss;
    oss << "{\n"
        << "  \"profileId\": \"" << p.profileId << "\",\n"
        << "  \"verified\": " << (p.isHardwareVerified ? "true" : "false") << ",\n"
        << "  \"manufacturer\": \"" << p.manufacturer << "\",\n"
        << "  \"marketingName\": \"" << p.marketingName << "\",\n"
        << "  \"socName\": \"" << p.socName << "\",\n"
        << "  \"primarySensorName\": \"" << p.primarySensorName << "\",\n"
        << "  \"storageTier\": \"" << p.storageTier << "\",\n"
        << "  \"activeBitDepth\": " << p.activeBitDepth << ",\n"
        << "  \"whiteLevel\": " << p.whiteLevel << ",\n"
        << "  \"rollingShutterReadout4kMs\": " << p.rollingShutterReadout4kMs << ",\n"
        << "  \"baseNativeIso\": " << p.baseNativeIso << ",\n"
        << "  \"dcgJumpIso\": " << p.dcgJumpIso << ",\n"
        << "  \"maxDynamicRangeStops\": " << p.maxDynamicRangeStops << ",\n"
        << "  \"quirkForceCpuImageReaderMap\": " << (p.quirkForceCpuImageReaderMap ? "true" : "false") << ",\n"
        << "  \"quirkRowStridePaddingOnRaw10\": " << (p.quirkRowStridePaddingOnRaw10 ? "true" : "false") << ",\n"
        << "  \"quirkPdafRowDefectFilterRequired\": " << (p.quirkPdafRowDefectFilterRequired ? "true" : "false") << ",\n"
        << "  \"maxSustainedStorageMBps\": " << p.maxSustainedStorageMBps << "\n"
        << "}";
    const std::string json = oss.str();
    return env->NewStringUTF(json.c_str());
}

JNIEXPORT void JNICALL
Java_com_example_engine_NativeCameraOsBridge_nativeReportAdpfActualWorkDurationNs(
    JNIEnv* /*env*/,
    jobject /*thiz*/,
    jlong actualDurationNs) {
    g_adpfMgr.reportActualWorkDuration(static_cast<int64_t>(actualDurationNs));
}

JNIEXPORT jboolean JNICALL
Java_com_example_engine_NativeCameraOsBridge_nativeGradeRgbaToArgbIntArray(
    JNIEnv* env,
    jobject /*thiz*/,
    jbyteArray srcRgbaJ,
    jintArray dstArgbJ,
    jint width,
    jint height,
    jfloat liftR,
    jfloat liftG,
    jfloat liftB,
    jfloat invGamma,
    jfloat gain,
    jfloat saturation,
    jfloat hdrStrength) {
    if (srcRgbaJ == nullptr || dstArgbJ == nullptr || width <= 0 || height <= 0) {
        return JNI_FALSE;
    }
    const jsize totalPixels = width * height;
    if (env->GetArrayLength(srcRgbaJ) < totalPixels * 4 ||
        env->GetArrayLength(dstArgbJ) < totalPixels) {
        return JNI_FALSE;
    }

    // Precompute 256-entry 1D transfer LUTs per channel (256 pow() calls instead of millions)
    uint8_t lutR[256];
    uint8_t lutG[256];
    uint8_t lutB[256];
    const float safeInvGamma = std::max(0.25f, invGamma);
    for (int c = 0; c < 256; ++c) {
        const float norm = static_cast<float>(c) * (1.0f / 255.0f);
        float r = (norm + liftR) * gain;
        float g = (norm + liftG) * gain;
        float b = (norm + liftB) * gain;
        if (hdrStrength > 0.02f) {
            const float k1 = hdrStrength * 0.6f;
            const float k2 = 1.0f + hdrStrength * 0.4f;
            r = (r / (1.0f + r * k1)) * k2;
            g = (g / (1.0f + g * k1)) * k2;
            b = (b / (1.0f + b * k1)) * k2;
        }
        r = std::pow(std::clamp(r, 0.0f, 1.0f), safeInvGamma);
        g = std::pow(std::clamp(g, 0.0f, 1.0f), safeInvGamma);
        b = std::pow(std::clamp(b, 0.0f, 1.0f), safeInvGamma);
        lutR[c] = static_cast<uint8_t>(std::clamp(static_cast<int>(r * 255.0f + 0.5f), 0, 255));
        lutG[c] = static_cast<uint8_t>(std::clamp(static_cast<int>(g * 255.0f + 0.5f), 0, 255));
        lutB[c] = static_cast<uint8_t>(std::clamp(static_cast<int>(b * 255.0f + 0.5f), 0, 255));
    }

    const int32_t satFixed = static_cast<int32_t>(saturation * 256.0f);

    auto* srcBytes = static_cast<const uint8_t*>(env->GetPrimitiveArrayCritical(srcRgbaJ, nullptr));
    auto* dstPixels = static_cast<jint*>(env->GetPrimitiveArrayCritical(dstArgbJ, nullptr));
    if (srcBytes == nullptr || dstPixels == nullptr) {
        if (srcBytes != nullptr) env->ReleasePrimitiveArrayCritical(srcRgbaJ, const_cast<uint8_t*>(srcBytes), JNI_ABORT);
        if (dstPixels != nullptr) env->ReleasePrimitiveArrayCritical(dstArgbJ, dstPixels, JNI_ABORT);
        return JNI_FALSE;
    }

    for (jsize i = 0; i < totalPixels; ++i) {
        const jsize base = i * 4;
        const int32_t r = lutR[srcBytes[base]];
        const int32_t g = lutG[srcBytes[base + 1]];
        const int32_t b = lutB[srcBytes[base + 2]];

        const int32_t luma = (54 * r + 183 * g + 19 * b) >> 8;
        const int32_t ro = std::clamp(luma + (((r - luma) * satFixed) >> 8), 0, 255);
        const int32_t go = std::clamp(luma + (((g - luma) * satFixed) >> 8), 0, 255);
        const int32_t bo = std::clamp(luma + (((b - luma) * satFixed) >> 8), 0, 255);

        dstPixels[i] = static_cast<jint>((0xFFu << 24) | (static_cast<uint32_t>(ro) << 16) |
                                         (static_cast<uint32_t>(go) << 8) | static_cast<uint32_t>(bo));
    }

    env->ReleasePrimitiveArrayCritical(dstArgbJ, dstPixels, 0);
    env->ReleasePrimitiveArrayCritical(srcRgbaJ, const_cast<uint8_t*>(srcBytes), JNI_ABORT);
    return JNI_TRUE;
}

} // extern "C"

#endif // defined(__ANDROID__)
