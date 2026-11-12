#include "apexcine/lp_platform/StorageBenchmarkAndIoProber.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

namespace apexcine::lp {

namespace {
bool probeIoUringSyscall() noexcept {
#if defined(__NR_io_uring_setup)
    // Pass 0 entries to __NR_io_uring_setup:
    // If kernel & seccomp allow the syscall, it returns -EINVAL (invalid entries=0).
    // If blocked by seccomp/SELinux or absent kernel config, it returns -ENOSYS or -EPERM/-EACCES.
    struct DummyParams {
        uint32_t words[32];
    } params{};
    long ret = ::syscall(__NR_io_uring_setup, 0u, &params);
    if (ret < 0 && errno == EINVAL) {
        return true;
    }
#endif
    return false;
}
} // namespace

StorageProbeResult StorageBenchmarkAndIoProber::probeAndBenchmarkDirectory(
    const std::string& directoryPath,
    size_t benchmarkTotalBytes) {
    StorageProbeResult result{};
    result.optimalBlockAlignmentBytes = 16384; // 16KB aligned for both 4K and 16K page kernels
    result.optimalWriteChunkBytes = 2 * 1024 * 1024; // 2 MiB write chunks

    struct statvfs vfs {};
    if (::statvfs(directoryPath.c_str(), &vfs) == 0) {
        result.availableFreeBytes =
            static_cast<uint64_t>(vfs.f_bavail) * static_cast<uint64_t>(vfs.f_frsize);
    }

    result.ioUringSupported = probeIoUringSyscall();

    const std::string probeFile = directoryPath + "/.apexcine_io_probe.tmp";
    const size_t chunkSize = std::min<size_t>(benchmarkTotalBytes, 2 * 1024 * 1024);
    const size_t totalBytes = std::max<size_t>(chunkSize, benchmarkTotalBytes);

    void* alignedBuf = nullptr;
    if (::posix_memalign(&alignedBuf, result.optimalBlockAlignmentBytes, chunkSize) != 0 ||
        alignedBuf == nullptr) {
        result.diagnosticSummary = "ERROR: Failed to allocate aligned IO benchmark slab";
        return result;
    }
    std::memset(alignedBuf, 0xA5, chunkSize);

    // 1. Probe O_DIRECT first
    int fd = -1;
#if defined(O_DIRECT)
    fd = ::open(probeFile.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT, 0644);
    if (fd >= 0) {
        struct iovec iov {
            alignedBuf, 16384
        };
        if (::pwritev(fd, &iov, 1, 0) == 16384) {
            result.oDirectSupported = true;
        } else {
            ::close(fd);
            fd = -1;
        }
    }
#endif

    // 2. Fallback to standard open if O_DIRECT was rejected by filesystem (e.g. tmpfs / F2FS compressed)
    if (fd < 0) {
        fd = ::open(probeFile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    }

    if (fd >= 0) {
#if defined(_GNU_SOURCE) || defined(__linux__)
        if (::posix_fallocate(fd, 0, static_cast<off_t>(totalBytes)) == 0) {
            result.fallocateSupported = true;
        }
#endif
        const auto start = std::chrono::steady_clock::now();
        size_t writtenTotal = 0;
        while (writtenTotal < totalBytes) {
            const size_t toWrite = std::min<size_t>(chunkSize, totalBytes - writtenTotal);
            struct iovec iov {
                alignedBuf, toWrite
            };
            const ssize_t w = ::pwritev(fd, &iov, 1, static_cast<off_t>(writtenTotal));
            if (w <= 0) {
                break;
            }
            writtenTotal += static_cast<size_t>(w);
        }
        // Single fdatasync at end of benchmark (never per-frame fsync in production!)
        (void)::fdatasync(fd);
        const auto end = std::chrono::steady_clock::now();
        const double elapsedSec = std::chrono::duration<double>(end - start).count();
        if (elapsedSec > 1e-6 && writtenTotal > 0) {
            result.measuredSequentialWriteMBps =
                (static_cast<double>(writtenTotal) / (1024.0 * 1024.0)) / elapsedSec;
        }
        ::close(fd);
        (void)::unlink(probeFile.c_str());
    }

    std::free(alignedBuf);

    result.isSustainedBeyondCache =
        result.oDirectSupported || (totalBytes >= 256ULL * 1024ULL * 1024ULL);

    if (result.ioUringSupported && result.oDirectSupported) {
        result.selectedBackend = IoWriteBackend::IO_URING_DIRECT;
    } else if (result.oDirectSupported) {
        result.selectedBackend = IoWriteBackend::O_DIRECT_PWRITEV;
    } else {
        result.selectedBackend = IoWriteBackend::PWRITEV_BUFFERED;
    }

    std::ostringstream oss;
    oss << "IOProbe[fallocate=" << (result.fallocateSupported ? "YES" : "NO")
        << ", O_DIRECT=" << (result.oDirectSupported ? "YES" : "FALLBACK_PWRITEV")
        << ", io_uring=" << (result.ioUringSupported ? "YES" : "BLOCKED_OR_FALLBACK")
        << ", Mode=" << (result.isSustainedBeyondCache ? "DIRECT_OR_LARGE" : "SHORT_BURST_CACHE")
        << ", WriteMBps=" << result.measuredSequentialWriteMBps << "]";
    result.diagnosticSummary = oss.str();
    return result;
}

} // namespace apexcine::lp
