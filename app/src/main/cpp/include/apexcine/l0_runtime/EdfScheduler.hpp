#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace apexcine::l0 {

using EdfTaskFn = void (*)(void* userContext) noexcept;

struct EdfTask {
    int64_t deadlineTimestampNs{0};
    uint64_t frameSequenceId{0};
    EdfTaskFn fn{nullptr};
    void* userContext{nullptr};
    uint8_t stageId{0};
};

struct EdfSchedulerStats {
    uint64_t tasksSubmitted{0};
    uint64_t tasksExecuted{0};
    uint64_t deadlineMisses{0};
    int64_t maxExecutionDurationNs{0};
    bool primeCoreAffinityApplied{false};
    bool urgentDisplayPriorityApplied{false};
    std::vector<int> detectedPrimeCores{};
};

/**
 * Layer 0: Earliest-Deadline-First (EDF) Real-Time Scheduler & CPU Affinity Manager.
 * - Pre-allocated fixed-capacity task heap (zero per-frame heap allocation).
 * - Orders tasks strictly by earliest deadlineTimestampNs.
 * - Discovers big/prime CPU cores via sysfs (/sys/devices/system/cpu/cpuX/cpufreq/cpuinfo_max_freq)
 *   and pins worker threads via sched_setaffinity() + setpriority(PRIO_PROCESS, 0, -8).
 */
class EdfScheduler {
public:
    static constexpr size_t kMaxPendingTasks = 128;

    EdfScheduler() noexcept;
    ~EdfScheduler() noexcept = default;

    /**
     * Discovers high-frequency prime/big CPU cores and applies thread affinity & urgent priority
     * to the calling worker thread. Safe fallback on constrained sandboxes.
     */
    bool pinCurrentThreadToPrimeCores(bool urgentDisplayPriority = true) noexcept;

    /**
     * Submits a task into the fixed-capacity EDF min-heap.
     * Zero heap allocation.
     */
    bool submitTask(const EdfTask& task) noexcept;

    /**
     * Pops and executes the task with the earliest deadline.
     * Returns true if a task was executed.
     */
    bool executeNextEarliest(int64_t currentTimestampNs) noexcept;

    /**
     * Drains all queued tasks in strict Earliest-Deadline-First order.
     */
    size_t drainAll(int64_t currentTimestampNs) noexcept;

    size_t pendingCount() const noexcept { return heapSize_; }

    EdfSchedulerStats getStats() const;

private:
    void siftUp(size_t index) noexcept;
    void siftDown(size_t index) noexcept;

    std::array<EdfTask, kMaxPendingTasks> minHeap_{};
    size_t heapSize_{0};
    std::atomic<uint64_t> tasksSubmitted_{0};
    std::atomic<uint64_t> tasksExecuted_{0};
    std::atomic<uint64_t> deadlineMisses_{0};
    std::atomic<int64_t> maxExecutionNs_{0};
    bool primeAffinitySet_{false};
    bool urgentPrioritySet_{false};
    std::vector<int> primeCores_{};
};

} // namespace apexcine::l0
