#include "apexcine/l0_runtime/EdfScheduler.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

namespace apexcine::l0 {

namespace {
std::vector<int> discoverPrimeCpuCores() {
    long numCores = ::sysconf(_SC_NPROCESSORS_CONF);
    if (numCores <= 0 || numCores > 64) {
        numCores = 8;
    }

    struct CoreFreq {
        int coreId;
        long maxFreqKhz;
    };
    std::vector<CoreFreq> freqs;
    freqs.reserve(static_cast<size_t>(numCores));
    long highestFreq = 0;

    for (int i = 0; i < static_cast<int>(numCores); ++i) {
        char path[128];
        std::snprintf(path,
                      sizeof(path),
                      "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq",
                      i);
        long freq = 0;
        if (FILE* f = std::fopen(path, "r")) {
            if (std::fscanf(f, "%ld", &freq) != 1) {
                freq = 0;
            }
            std::fclose(f);
        }
        highestFreq = std::max(highestFreq, freq);
        freqs.push_back({i, freq});
    }

    std::vector<int> selected;
    if (highestFreq > 0) {
        // Select cores within 75% of peak prime frequency (big + prime cluster)
        const long threshold = (highestFreq * 75) / 100;
        for (const auto& cf : freqs) {
            if (cf.maxFreqKhz >= threshold) {
                selected.push_back(cf.coreId);
            }
        }
    }

    if (selected.empty()) {
        // Fallback: upper half of cores (typically big/prime on ARM DynamIQ)
        const int startCore = (numCores >= 4) ? static_cast<int>(numCores / 2) : 0;
        for (int i = startCore; i < static_cast<int>(numCores); ++i) {
            selected.push_back(i);
        }
    }
    return selected;
}
} // namespace

EdfScheduler::EdfScheduler() noexcept {
    primeCores_ = discoverPrimeCpuCores();
}

bool EdfScheduler::pinCurrentThreadToPrimeCores(bool urgentDisplayPriority) noexcept {
    if (urgentDisplayPriority) {
        // Android THREAD_PRIORITY_URGENT_DISPLAY corresponds to nice value -8
        if (::setpriority(PRIO_PROCESS, 0, -8) == 0) {
            urgentPrioritySet_ = true;
        }
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core : primeCores_) {
        CPU_SET(core, &cpuset);
    }
    if (::sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0) {
        primeAffinitySet_ = true;
    }
    return primeAffinitySet_;
}

bool EdfScheduler::submitTask(const EdfTask& task) noexcept {
    if (task.fn == nullptr || heapSize_ >= kMaxPendingTasks) {
        return false;
    }
    minHeap_[heapSize_] = task;
    siftUp(heapSize_);
    ++heapSize_;
    tasksSubmitted_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool EdfScheduler::executeNextEarliest(int64_t currentTimestampNs) noexcept {
    if (heapSize_ == 0) {
        return false;
    }

    const EdfTask task = minHeap_[0];
    --heapSize_;
    if (heapSize_ > 0) {
        minHeap_[0] = minHeap_[heapSize_];
        siftDown(0);
    }

    const auto start = std::chrono::steady_clock::now();
    task.fn(task.userContext);
    const auto end = std::chrono::steady_clock::now();
    const int64_t execNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    int64_t prevMax = maxExecutionNs_.load(std::memory_order_relaxed);
    while (execNs > prevMax &&
           !maxExecutionNs_.compare_exchange_weak(prevMax, execNs, std::memory_order_relaxed)) {
    }

    if ((currentTimestampNs + execNs) > task.deadlineTimestampNs) {
        deadlineMisses_.fetch_add(1, std::memory_order_relaxed);
    }
    tasksExecuted_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

size_t EdfScheduler::drainAll(int64_t currentTimestampNs) noexcept {
    size_t count = 0;
    const auto batchStart = std::chrono::steady_clock::now();
    while (heapSize_ > 0) {
        const auto now = std::chrono::steady_clock::now();
        const int64_t elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - batchStart).count();
        if (!executeNextEarliest(currentTimestampNs + elapsedNs)) {
            break;
        }
        ++count;
    }
    return count;
}

void EdfScheduler::siftUp(size_t index) noexcept {
    while (index > 0) {
        const size_t parent = (index - 1) / 2;
        if (minHeap_[index].deadlineTimestampNs < minHeap_[parent].deadlineTimestampNs) {
            std::swap(minHeap_[index], minHeap_[parent]);
            index = parent;
        } else {
            break;
        }
    }
}

void EdfScheduler::siftDown(size_t index) noexcept {
    while (true) {
        const size_t left = 2 * index + 1;
        const size_t right = 2 * index + 2;
        size_t smallest = index;

        if (left < heapSize_ &&
            minHeap_[left].deadlineTimestampNs < minHeap_[smallest].deadlineTimestampNs) {
            smallest = left;
        }
        if (right < heapSize_ &&
            minHeap_[right].deadlineTimestampNs < minHeap_[smallest].deadlineTimestampNs) {
            smallest = right;
        }
        if (smallest != index) {
            std::swap(minHeap_[index], minHeap_[smallest]);
            index = smallest;
        } else {
            break;
        }
    }
}

EdfSchedulerStats EdfScheduler::getStats() const {
    EdfSchedulerStats stats{};
    stats.tasksSubmitted = tasksSubmitted_.load(std::memory_order_relaxed);
    stats.tasksExecuted = tasksExecuted_.load(std::memory_order_relaxed);
    stats.deadlineMisses = deadlineMisses_.load(std::memory_order_relaxed);
    stats.maxExecutionDurationNs = maxExecutionNs_.load(std::memory_order_relaxed);
    stats.primeCoreAffinityApplied = primeAffinitySet_;
    stats.urgentDisplayPriorityApplied = urgentPrioritySet_;
    stats.detectedPrimeCores = primeCores_;
    return stats;
}

} // namespace apexcine::l0
