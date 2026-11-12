#if defined(__ANDROID__)

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

} // extern "C"

#endif // defined(__ANDROID__)
