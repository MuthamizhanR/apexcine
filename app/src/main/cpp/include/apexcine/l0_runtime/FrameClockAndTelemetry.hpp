#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace apexcine::l0 {

/**
 * Glass-to-glass stage latency breakdown (target < 60ms, stretch < 40ms).
 */
struct FrameLatencySample {
    uint64_t frameSequenceId{0};
    int64_t sensorReadoutNs{0};       // Budget: 8 - 30 ms
    int64_t halDeliveryNs{0};         // Budget: 5 - 15 ms
    int64_t unpackPlanarizeNs{0};     // Budget: 1 - 3 ms
    int64_t gpuDebayerScopesNs{0};    // Budget: 2 - 5 ms
    int64_t compositionDisplayNs{0};  // Budget: 8 - 16 ms
    int64_t losslessCompressNs{0};    // Record path parallel branch
    int64_t diskWriteNs{0};           // Record path IO branch

    constexpr int64_t totalGlassToGlassNs() const noexcept {
        return sensorReadoutNs + halDeliveryNs + unpackPlanarizeNs +
               gpuDebayerScopesNs + compositionDisplayNs;
    }

    constexpr double totalGlassToGlassMs() const noexcept {
        return static_cast<double>(totalGlassToGlassNs()) / 1e6;
    }
};

struct TraceSpanEvent {
    const char* name{"unknown"};
    int64_t startUs{0};
    int64_t durationUs{0};
    uint32_t threadId{1};
    uint64_t frameId{0};
};

struct TelemetrySummary {
    uint64_t framesRecorded{0};
    uint64_t droppedFrames{0};
    uint64_t uiJankFramesOver120HzDeadline{0};
    uint64_t watchdogAlerts{0};
    double meanGlassToGlassMs{0.0};
    double p99GlassToGlassMs{0.0};
    double meanUnpackMs{0.0};
    double meanGpuDebayerMs{0.0};
    double meanCompressThroughputGBps{0.0};
    double meanCompressionRatio{0.0};
    bool meetsAllLatencyKpis{false};
};

/**
 * Layer 0: Monotonic Frame Clock, Watchdog & Perfetto/Systrace Telemetry Engine.
 */
class FrameClockAndTelemetry {
public:
    static constexpr size_t kHistoryCapacity = 256;
    static constexpr size_t kTraceSpanCapacity = 512;
    static constexpr int64_t kMaxGlassToGlassBudgetNs = 60'000'000LL; // 60 ms hard KPI
    static constexpr int64_t kStretchGlassToGlassNs = 40'000'000LL;   // 40 ms stretch KPI
    static constexpr int64_t k120HzFrameDeadlineNs = 8'333'333LL;     // 8.33 ms @ 120Hz

    FrameClockAndTelemetry() noexcept = default;
    ~FrameClockAndTelemetry() noexcept = default;

    static int64_t nowMonotonicNs() noexcept;
    static int64_t nowBoottimeNs() noexcept;

    void recordFrameLatency(const FrameLatencySample& sample,
                            int64_t uiRenderDurationNs,
                            size_t uncompressedBytes,
                            size_t compressedBytes) noexcept;

    void recordSpan(const char* spanName,
                    int64_t startNs,
                    int64_t endNs,
                    uint32_t threadId,
                    uint64_t frameId) noexcept;

    void recordDroppedFrame() noexcept {
        droppedFrames_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Checks whether the time elapsed since the last HAL frame delivery exceeds watchdogThresholdNs.
     */
    bool checkWatchdogStall(int64_t currentMonotonicNs, int64_t watchdogThresholdNs) noexcept;

    TelemetrySummary computeSummary() const noexcept;

    /**
     * Exports recorded spans as Perfetto / Chrome Trace Event Format JSON.
     */
    std::string exportPerfettoTraceJson() const;

private:
    alignas(64) std::atomic<uint64_t> writeIdx_{0};
    alignas(64) std::atomic<uint64_t> spanWriteIdx_{0};
    alignas(64) std::atomic<uint64_t> droppedFrames_{0};
    alignas(64) std::atomic<uint64_t> uiJankCount_{0};
    alignas(64) std::atomic<uint64_t> watchdogAlerts_{0};
    alignas(64) std::atomic<int64_t> lastFrameArrivalNs_{0};
    alignas(64) std::atomic<uint64_t> totalUncompressedBytes_{0};
    alignas(64) std::atomic<uint64_t> totalCompressedBytes_{0};
    alignas(64) std::atomic<uint64_t> totalCompressTimeNs_{0};

    mutable std::mutex ringMutex_{};
    std::array<FrameLatencySample, kHistoryCapacity> samples_{};
    std::array<TraceSpanEvent, kTraceSpanCapacity> spans_{};
};

} // namespace apexcine::l0
