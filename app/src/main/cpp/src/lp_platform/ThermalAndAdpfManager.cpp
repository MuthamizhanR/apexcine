#include "apexcine/lp_platform/ThermalAndAdpfManager.hpp"

#include <dlfcn.h>
#include <sstream>
#include <unistd.h>

namespace apexcine::lp {

namespace {
using PFN_APerformanceHint_getManager = void* (*)();
using PFN_APerformanceHint_createSession = void* (*)(void* manager,
                                                     const int32_t* threadIds,
                                                     size_t size,
                                                     int64_t initialTargetWorkDurationNanos);
using PFN_APerformanceHint_reportActualWorkDuration = int (*)(void* session,
                                                              int64_t actualDurationNanos);
using PFN_APerformanceHint_updateTargetWorkDuration = int (*)(void* session,
                                                              int64_t targetDurationNanos);
using PFN_APerformanceHint_closeSession = void (*)(void* session);
} // namespace

ThermalAndAdpfManager::ThermalAndAdpfManager(int64_t initialTargetDurationNs) noexcept
    : targetWorkDurationNs_(initialTargetDurationNs) {
    // Resolve Android NDK APerformanceHint symbols dynamically (API 33+ in libandroid.so)
    void* getMgrSym = ::dlsym(RTLD_DEFAULT, "APerformanceHint_getManager");
    void* createSym = ::dlsym(RTLD_DEFAULT, "APerformanceHint_createSession");
    fnReportActual_ = ::dlsym(RTLD_DEFAULT, "APerformanceHint_reportActualWorkDuration");
    fnUpdateTarget_ = ::dlsym(RTLD_DEFAULT, "APerformanceHint_updateTargetWorkDuration");
    fnCloseSession_ = ::dlsym(RTLD_DEFAULT, "APerformanceHint_closeSession");

    if (getMgrSym != nullptr && createSym != nullptr) {
        auto getMgr = reinterpret_cast<PFN_APerformanceHint_getManager>(getMgrSym);
        adpfManagerHandle_ = getMgr();
    }
}

ThermalAndAdpfManager::~ThermalAndAdpfManager() noexcept {
    if (adpfSessionHandle_ != nullptr && fnCloseSession_ != nullptr) {
        auto closeFn = reinterpret_cast<PFN_APerformanceHint_closeSession>(fnCloseSession_);
        closeFn(adpfSessionHandle_);
        adpfSessionHandle_ = nullptr;
    }
}

bool ThermalAndAdpfManager::initializeAdpfSession(const std::vector<int32_t>& threadIds) noexcept {
    void* createSym = ::dlsym(RTLD_DEFAULT, "APerformanceHint_createSession");
    if (adpfManagerHandle_ != nullptr && createSym != nullptr && !threadIds.empty()) {
        auto createFn = reinterpret_cast<PFN_APerformanceHint_createSession>(createSym);
        adpfSessionHandle_ = createFn(adpfManagerHandle_,
                                      threadIds.data(),
                                      threadIds.size(),
                                      targetWorkDurationNs_.load(std::memory_order_relaxed));
        return adpfSessionHandle_ != nullptr;
    }
    return false;
}

void ThermalAndAdpfManager::updateTargetWorkDuration(int64_t targetDurationNs) noexcept {
    targetWorkDurationNs_.store(targetDurationNs, std::memory_order_relaxed);
    if (adpfSessionHandle_ != nullptr && fnUpdateTarget_ != nullptr) {
        auto updateFn =
            reinterpret_cast<PFN_APerformanceHint_updateTargetWorkDuration>(fnUpdateTarget_);
        (void)updateFn(adpfSessionHandle_, targetDurationNs);
    }
}

void ThermalAndAdpfManager::reportActualWorkDuration(int64_t actualDurationNs) noexcept {
    lastActualWorkDurationNs_.store(actualDurationNs, std::memory_order_relaxed);
    if (adpfSessionHandle_ != nullptr && fnReportActual_ != nullptr) {
        auto reportFn =
            reinterpret_cast<PFN_APerformanceHint_reportActualWorkDuration>(fnReportActual_);
        (void)reportFn(adpfSessionHandle_, actualDurationNs);
    }
}

AdpfAndThermalReport ThermalAndAdpfManager::evaluateThermalPolicy(
    float currentHeadroom,
    ThermalState osThermalState) noexcept {
    thermalHeadroom_.store(currentHeadroom, std::memory_order_relaxed);
    thermalState_.store(osThermalState, std::memory_order_relaxed);

    QoSDegradationStage stage = QoSDegradationStage::STAGE_0_FULL_FIDELITY_120HZ;
    if (osThermalState >= ThermalState::SEVERE || currentHeadroom >= 0.95f) {
        stage = QoSDegradationStage::STAGE_3_SHED_PROXY_ENCODE;
    } else if (osThermalState == ThermalState::MODERATE || currentHeadroom >= 0.85f) {
        stage = QoSDegradationStage::STAGE_2_SHED_HEAVY_SCOPES;
    } else if (osThermalState == ThermalState::LIGHT || currentHeadroom >= 0.75f) {
        stage = QoSDegradationStage::STAGE_1_HALF_RES_PREVIEW;
    }
    qosStage_.store(stage, std::memory_order_relaxed);

    return currentReport();
}

AdpfAndThermalReport ThermalAndAdpfManager::currentReport() const {
    AdpfAndThermalReport rep{};
    rep.adpfHardwareSessionActive = (adpfSessionHandle_ != nullptr);
    rep.targetWorkDurationNs = targetWorkDurationNs_.load(std::memory_order_relaxed);
    rep.lastReportedActualWorkDurationNs = lastActualWorkDurationNs_.load(std::memory_order_relaxed);
    rep.thermalHeadroom = thermalHeadroom_.load(std::memory_order_relaxed);
    rep.thermalState = thermalState_.load(std::memory_order_relaxed);
    rep.qosStage = qosStage_.load(std::memory_order_relaxed);
    rep.rawRecordingZeroDropGuaranteed = true; // Invariant: never drop recorded RAW frames

    switch (rep.qosStage) {
        case QoSDegradationStage::STAGE_0_FULL_FIDELITY_120HZ:
            rep.previewDownsampleFactor = 1;
            rep.scopesWaveformEnabled = true;
            rep.liveProxyEncodeEnabled = true;
            rep.policySummary =
                "NOMINAL: 120Hz UI + Full Preview Debayer + Full Scopes + Live Proxy + Zero-Drop RAW";
            break;
        case QoSDegradationStage::STAGE_1_HALF_RES_PREVIEW:
            rep.previewDownsampleFactor = 2;
            rep.scopesWaveformEnabled = true;
            rep.liveProxyEncodeEnabled = true;
            rep.policySummary =
                "LIGHT THERMAL GUARD: 1/2-Res Preview Debayer + Scopes + Live Proxy + Zero-Drop RAW";
            break;
        case QoSDegradationStage::STAGE_2_SHED_HEAVY_SCOPES:
            rep.previewDownsampleFactor = 2;
            rep.scopesWaveformEnabled = false;
            rep.liveProxyEncodeEnabled = true;
            rep.policySummary =
                "MODERATE THERMAL GUARD: Shed Waveform/Parade Compute, Keep ETTR + Zero-Drop RAW";
            break;
        case QoSDegradationStage::STAGE_3_SHED_PROXY_ENCODE:
            rep.previewDownsampleFactor = 4;
            rep.scopesWaveformEnabled = false;
            rep.liveProxyEncodeEnabled = false;
            rep.policySummary =
                "SEVERE THERMAL GUARD: Paused Live Proxy & Heavy Scopes; 100% Zero-Drop RAW Preserved";
            break;
    }
    return rep;
}

} // namespace apexcine::lp
