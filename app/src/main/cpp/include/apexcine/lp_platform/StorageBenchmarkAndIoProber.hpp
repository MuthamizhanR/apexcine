#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace apexcine::lp {

enum class IoWriteBackend : uint8_t {
    PWRITEV_BUFFERED = 0, // Universal fallback
    O_DIRECT_PWRITEV = 1, // Page-aligned zero-page-cache sequential write
    IO_URING_DIRECT = 2   // Kernel async ring (when permitted by Android SELinux/seccomp)
};

struct StorageProbeResult {
    bool fallocateSupported{false};
    bool oDirectSupported{false};
    bool ioUringSupported{false};
    bool isSustainedBeyondCache{false}; // True only when O_DIRECT is active or benchmark >= 256 MiB
    IoWriteBackend selectedBackend{IoWriteBackend::PWRITEV_BUFFERED};
    size_t optimalBlockAlignmentBytes{4096};
    size_t optimalWriteChunkBytes{2 * 1024 * 1024}; // 2 MiB sequential blocks
    double measuredSequentialWriteMBps{0.0};
    uint64_t availableFreeBytes{0};
    std::string diagnosticSummary{};

    /**
     * Refuses configs that exceed 85% of the volume's measured sequential write rate.
     */
    constexpr bool canSustainCaptureRate(double requiredCompressedMBps) const noexcept {
        return measuredSequentialWriteMBps >= (requiredCompressedMBps * 1.15);
    }
};

/**
 * Layer LP: Storage Volume Benchmark & Kernel IO Prober.
 *
 * - Benchmarks target directory on first run using aligned 1-4 MiB blocks.
 * - Probes fallocate(), O_DIRECT, and io_uring syscall availability (catching ENOSYS/EPERM
 *   when blocked by Android seccomp/SELinux) and selects the fastest safe fallback (pwritev).
 * - Never uses SAF/FUSE on the hot path.
 */
class StorageBenchmarkAndIoProber {
public:
    StorageBenchmarkAndIoProber() = default;
    ~StorageBenchmarkAndIoProber() = default;

    static StorageProbeResult probeAndBenchmarkDirectory(
        const std::string& directoryPath,
        size_t benchmarkTotalBytes = 8 * 1024 * 1024);
};

} // namespace apexcine::lp
