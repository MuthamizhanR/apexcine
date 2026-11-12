#include "apexcine/l0_runtime/FrameClockAndTelemetry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <vector>

#if defined(__ANDROID__)
#include <android/trace.h>
#endif

namespace apexcine::l0 {

int64_t FrameClockAndTelemetry::nowMonotonicNs() noexcept {
    struct timespec ts {};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + static_cast<int64_t>(ts.tv_nsec);
}

int64_t FrameClockAndTelemetry::nowBoottimeNs() noexcept {
    struct timespec ts {};
#if defined(CLOCK_BOOTTIME)
    ::clock_gettime(CLOCK_BOOTTIME, &ts);
#else
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + static_cast<int64_t>(ts.tv_nsec);
}

void FrameClockAndTelemetry::recordFrameLatency(const FrameLatencySample& sample,
                                                int64_t uiRenderDurationNs,
                                                size_t uncompressedBytes,
                                                size_t compressedBytes) noexcept {
    {
        std::lock_guard<std::mutex> lock(ringMutex_);
        const uint64_t idx = writeIdx_.fetch_add(1, std::memory_order_relaxed);
        samples_[idx % kHistoryCapacity] = sample;
    }

    if (uiRenderDurationNs > k120HzFrameDeadlineNs) {
        uiJankCount_.fetch_add(1, std::memory_order_relaxed);
    }

    totalUncompressedBytes_.fetch_add(uncompressedBytes, std::memory_order_relaxed);
    totalCompressedBytes_.fetch_add(compressedBytes, std::memory_order_relaxed);
    totalCompressTimeNs_.fetch_add(
        static_cast<uint64_t>(std::max<int64_t>(sample.losslessCompressNs, 1)),
        std::memory_order_relaxed);
    lastFrameArrivalNs_.store(nowMonotonicNs(), std::memory_order_release);

#if defined(__ANDROID__)
    if (ATrace_isEnabled()) {
        ATrace_setCounter("ApexCine_GlassToGlassUs", sample.totalGlassToGlassNs() / 1000);
        ATrace_setCounter("ApexCine_DroppedFrames",
                          static_cast<int64_t>(droppedFrames_.load(std::memory_order_relaxed)));
    }
#endif
}

void FrameClockAndTelemetry::recordSpan(const char* spanName,
                                        int64_t startNs,
                                        int64_t endNs,
                                        uint32_t threadId,
                                        uint64_t frameId) noexcept {
    TraceSpanEvent ev{};
    ev.name = (spanName != nullptr) ? spanName : "span";
    ev.startUs = startNs / 1000;
    ev.durationUs = std::max<int64_t>(1, (endNs - startNs) / 1000);
    ev.threadId = threadId;
    ev.frameId = frameId;

    std::lock_guard<std::mutex> lock(ringMutex_);
    const uint64_t idx = spanWriteIdx_.fetch_add(1, std::memory_order_relaxed);
    spans_[idx % kTraceSpanCapacity] = ev;
}

bool FrameClockAndTelemetry::checkWatchdogStall(int64_t currentMonotonicNs,
                                                int64_t watchdogThresholdNs) noexcept {
    const int64_t lastArrival = lastFrameArrivalNs_.load(std::memory_order_acquire);
    if (lastArrival == 0) {
        return false;
    }
    if ((currentMonotonicNs - lastArrival) > watchdogThresholdNs) {
        watchdogAlerts_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

TelemetrySummary FrameClockAndTelemetry::computeSummary() const noexcept {
    TelemetrySummary out{};
    std::vector<FrameLatencySample> snapshot;
    uint64_t totalFrames = 0;
    {
        std::lock_guard<std::mutex> lock(ringMutex_);
        totalFrames = writeIdx_.load(std::memory_order_relaxed);
        const size_t validCount =
            static_cast<size_t>(std::min<uint64_t>(totalFrames, kHistoryCapacity));
        snapshot.assign(samples_.begin(), samples_.begin() + validCount);
    }

    out.framesRecorded = totalFrames;
    out.droppedFrames = droppedFrames_.load(std::memory_order_relaxed);
    out.uiJankFramesOver120HzDeadline = uiJankCount_.load(std::memory_order_relaxed);
    out.watchdogAlerts = watchdogAlerts_.load(std::memory_order_relaxed);

    const size_t validCount = snapshot.size();
    if (validCount > 0) {
        std::vector<double> g2gMs;
        g2gMs.reserve(validCount);
        double sumG2g = 0.0;
        double sumUnpack = 0.0;
        double sumGpu = 0.0;

        for (size_t i = 0; i < validCount; ++i) {
            const auto& s = snapshot[i];
            const double g2g = s.totalGlassToGlassMs();
            g2gMs.push_back(g2g);
            sumG2g += g2g;
            sumUnpack += static_cast<double>(s.unpackPlanarizeNs) / 1e6;
            sumGpu += static_cast<double>(s.gpuDebayerScopesNs) / 1e6;
        }
        std::sort(g2gMs.begin(), g2gMs.end());
        out.meanGlassToGlassMs = sumG2g / static_cast<double>(validCount);
        const size_t p99Idx = std::min<size_t>((validCount * 99) / 100, validCount - 1);
        out.p99GlassToGlassMs = g2gMs[p99Idx];
        out.meanUnpackMs = sumUnpack / static_cast<double>(validCount);
        out.meanGpuDebayerMs = sumGpu / static_cast<double>(validCount);
    }

    const uint64_t uncomp = totalUncompressedBytes_.load(std::memory_order_relaxed);
    const uint64_t comp = totalCompressedBytes_.load(std::memory_order_relaxed);
    const uint64_t compNs = totalCompressTimeNs_.load(std::memory_order_relaxed);

    if (comp > 0) {
        out.meanCompressionRatio = static_cast<double>(uncomp) / static_cast<double>(comp);
    }
    if (compNs > 0) {
        // Bytes / ns == GB / s
        out.meanCompressThroughputGBps = static_cast<double>(uncomp) / static_cast<double>(compNs);
    }

    const double jankRatio =
        (totalFrames > 0)
            ? (static_cast<double>(out.uiJankFramesOver120HzDeadline) / static_cast<double>(totalFrames))
            : 0.0;

    out.meetsAllLatencyKpis = (out.droppedFrames == 0) &&
                              (out.meanGlassToGlassMs < 60.0) &&
                              (jankRatio < 0.01);
    return out;
}

std::string FrameClockAndTelemetry::exportPerfettoTraceJson() const {
    std::vector<TraceSpanEvent> snapshot;
    {
        std::lock_guard<std::mutex> lock(ringMutex_);
        const uint64_t totalSpans = spanWriteIdx_.load(std::memory_order_relaxed);
        const size_t count =
            static_cast<size_t>(std::min<uint64_t>(totalSpans, kTraceSpanCapacity));
        snapshot.assign(spans_.begin(), spans_.begin() + count);
    }

    auto escapeJson = [](const char* raw) -> std::string {
        if (raw == nullptr) return "span";
        std::string out;
        for (const char* p = raw; *p != '\0'; ++p) {
            if (*p == '"' || *p == '\\') {
                out.push_back('\\');
                out.push_back(*p);
            } else if (static_cast<unsigned char>(*p) >= 0x20) {
                out.push_back(*p);
            }
        }
        return out;
    };

    std::ostringstream oss;
    oss << "{\"traceEvents\":[\n";
    for (size_t i = 0; i < snapshot.size(); ++i) {
        const auto& ev = snapshot[i];
        if (i > 0) {
            oss << ",\n";
        }
        oss << "  {\"name\":\"" << escapeJson(ev.name) << "\",\"cat\":\"apexcine.pipeline\",\"ph\":\"X\","
            << "\"ts\":" << ev.startUs << ",\"dur\":" << ev.durationUs
            << ",\"pid\":1,\"tid\":" << ev.threadId
            << ",\"args\":{\"frameId\":" << ev.frameId << "}}";
    }
    oss << "\n]}";
    return oss.str();
}

} // namespace apexcine::l0
