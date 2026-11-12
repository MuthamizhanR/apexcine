#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace apexcine::lp {

enum class ThermalState : uint8_t {
    NOMINAL = 0,
    LIGHT = 1,
    MODERATE = 2,
    SEVERE = 3,
    CRITICAL = 4
};

enum class QoSDegradationStage : uint8_t {
    STAGE_0_FULL_FIDELITY_120HZ = 0, // Full preview + full scopes + live proxy + 100% RAW record
    STAGE_1_HALF_RES_PREVIEW = 1,    // Quarter/half-res preview debayer, 60Hz UI scopes, 100% RAW record
    STAGE_2_SHED_HEAVY_SCOPES = 2,   // Disable waveform/parade, keep ETTR histogram, 100% RAW record
    STAGE_3_SHED_PROXY_ENCODE = 3    // Pause background HEVC proxy; 100% RAW record preserved!
};

struct AdpfAndThermalReport {
    bool adpfHardwareSessionActive{false};
    int64_t targetWorkDurationNs{8'333'333LL};
    int64_t lastReportedActualWorkDurationNs{0};
    float thermalHeadroom{0.35f}; // < 1.0f means below throttling threshold
    ThermalState thermalState{ThermalState::NOMINAL};
    QoSDegradationStage qosStage{QoSDegradationStage::STAGE_0_FULL_FIDELITY_120HZ};
    uint32_t previewDownsampleFactor{1};
    bool scopesWaveformEnabled{true};
    bool liveProxyEncodeEnabled{true};
    bool rawRecordingZeroDropGuaranteed{true};
    std::string policySummary{};
};

/**
 * Layer LP: Android Dynamic Performance Framework (ADPF) & Thermal QoS Governor.
 *
 * - Dynamically resolves APerformanceHint APIs via dlsym() so minSdk 29 binaries run everywhere
 *   while activating hardware ADPF core boosting on Android 12/13+ (API 31/33+).
 * - Implements graceful degradation under thermal load:
 *   Preview resolution -> Scopes -> Proxy encode; NEVER drops recorded RAW frames.
 */
class ThermalAndAdpfManager {
public:
    explicit ThermalAndAdpfManager(int64_t initialTargetDurationNs = 8'333'333LL) noexcept;
    ~ThermalAndAdpfManager() noexcept;

    bool initializeAdpfSession(const std::vector<int32_t>& threadIds) noexcept;

    void updateTargetWorkDuration(int64_t targetDurationNs) noexcept;

    void reportActualWorkDuration(int64_t actualDurationNs) noexcept;

    /**
     * Evaluates thermal headroom (0.0 .. 1.2+, where 1.0 = severe throttling threshold)
     * and Android PowerManager thermal status, returning the deterministic QoS policy.
     */
    AdpfAndThermalReport evaluateThermalPolicy(float currentHeadroom,
                                               ThermalState osThermalState) noexcept;

    AdpfAndThermalReport currentReport() const;

private:
    void* adpfManagerHandle_{nullptr};
    void* adpfSessionHandle_{nullptr};
    void* fnReportActual_{nullptr};
    void* fnUpdateTarget_{nullptr};
    void* fnCloseSession_{nullptr};

    std::atomic<int64_t> targetWorkDurationNs_{8'333'333LL};
    std::atomic<int64_t> lastActualWorkDurationNs_{0};
    std::atomic<float> thermalHeadroom_{0.35f};
    std::atomic<ThermalState> thermalState_{ThermalState::NOMINAL};
    std::atomic<QoSDegradationStage> qosStage_{QoSDegradationStage::STAGE_0_FULL_FIDELITY_120HZ};
};

} // namespace apexcine::lp
