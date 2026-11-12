#include "apexcine/l0_runtime/ArenaAllocator.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>

namespace apexcine::l0 {

namespace {
constexpr size_t alignUp(size_t value, size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}
} // namespace

ArenaAllocator::ArenaAllocator(size_t slabCount, size_t slabSizeBytes)
    : slabCount_(std::min<size_t>( std::max<size_t>(slabCount, 1), kMaxSlabs)),
      alignedSlabSize_(alignUp(std::max<size_t>(slabSizeBytes, kPageAlignment), kPageAlignment)),
      totalAllocatedBytes_(slabCount_ * alignedSlabSize_) {
    // 1. Allocate 16KB page-aligned memory (try mmap with MAP_POPULATE first, fallback to posix_memalign)
#if defined(MAP_POPULATE)
    void* mapped = ::mmap(nullptr,
                          totalAllocatedBytes_,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
                          -1,
                          0);
    if (mapped != MAP_FAILED && mapped != nullptr &&
        (reinterpret_cast<uintptr_t>(mapped) & (kPageAlignment - 1)) == 0) {
        basePtr_ = static_cast<uint8_t*>(mapped);
        usesMmap_ = true;
        isPopulated_ = true;
    }
#endif

    if (basePtr_ == nullptr) {
        void* raw = nullptr;
        if (::posix_memalign(&raw, kPageAlignment, totalAllocatedBytes_) == 0 && raw != nullptr) {
            basePtr_ = static_cast<uint8_t*>(raw);
            usesMmap_ = false;
        }
    }

    if (basePtr_ != nullptr) {
        // Pre-touch every 4KB page to eliminate minor page faults during real-time capture
        for (size_t offset = 0; offset < totalAllocatedBytes_; offset += 4096) {
            basePtr_[offset] = 0;
        }
        isPopulated_ = true;

        // Hint kernel to keep pages resident
        (void)::madvise(basePtr_, totalAllocatedBytes_, MADV_WILLNEED);

        // Attempt mlock() to pin slabs in physical RAM; degrade gracefully if RLIMIT_MEMLOCK denies it
        if (::mlock(basePtr_, totalAllocatedBytes_) == 0) {
            isMlocked_ = true;
        } else {
            isMlocked_ = false;
        }
    }

    const uint64_t initialMask = (slabCount_ == 64) ? ~0ULL : ((1ULL << slabCount_) - 1ULL);
    freeMask_.store(initialMask, std::memory_order_relaxed);
}

ArenaAllocator::~ArenaAllocator() noexcept {
    if (basePtr_ != nullptr) {
        if (isMlocked_) {
            (void)::munlock(basePtr_, totalAllocatedBytes_);
        }
        if (usesMmap_) {
            (void)::munmap(basePtr_, totalAllocatedBytes_);
        } else {
            std::free(basePtr_);
        }
        basePtr_ = nullptr;
    }
}

bool ArenaAllocator::acquireSlab(ProducerStage producer,
                                 ConsumerStage consumer,
                                 BufferDescriptor& outDescriptor,
                                 uint8_t*& outPtr) noexcept {
    if (basePtr_ == nullptr) {
        exhaustionCount_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    uint64_t currentMask = freeMask_.load(std::memory_order_acquire);
    while (currentMask != 0ULL) {
        const uint32_t bitIndex = static_cast<uint32_t>(__builtin_ctzll(currentMask));
        const uint64_t clearBit = ~(1ULL << bitIndex);
        if (freeMask_.compare_exchange_weak(
                currentMask, currentMask & clearBit, std::memory_order_acq_rel, std::memory_order_acquire)) {
            outPtr = basePtr_ + (static_cast<size_t>(bitIndex) * alignedSlabSize_);
            outDescriptor.slabIndex = bitIndex;
            outDescriptor.capacityBytes = alignedSlabSize_;
            outDescriptor.validBytes = 0;
            outDescriptor.producer = producer;
            outDescriptor.consumer = consumer;
            outDescriptor.residency = MemoryResidency::CPU_LOCKED_ARENA;

            const size_t active = activeSlabs_.fetch_add(1, std::memory_order_relaxed) + 1;
            size_t prevPeak = peakActiveSlabs_.load(std::memory_order_relaxed);
            while (active > prevPeak &&
                   !peakActiveSlabs_.compare_exchange_weak(prevPeak, active, std::memory_order_relaxed)) {
            }
            totalAcquisitions_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    exhaustionCount_.fetch_add(1, std::memory_order_relaxed);
    outPtr = nullptr;
    return false;
}

bool ArenaAllocator::releaseSlab(uint32_t slabIndex) noexcept {
    if (slabIndex >= slabCount_) {
        return false;
    }
    const uint64_t bit = (1ULL << slabIndex);
    const uint64_t prev = freeMask_.fetch_or(bit, std::memory_order_acq_rel);
    if ((prev & bit) != 0ULL) {
        return false; // Already free (double-release guard)
    }
    activeSlabs_.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

uint8_t* ArenaAllocator::getSlabPtr(uint32_t slabIndex) const noexcept {
    if (basePtr_ == nullptr || slabIndex >= slabCount_) {
        return nullptr;
    }
    return basePtr_ + (static_cast<size_t>(slabIndex) * alignedSlabSize_);
}

ArenaStats ArenaAllocator::getStats() const noexcept {
    ArenaStats stats{};
    stats.slabCount = slabCount_;
    stats.slabSizeBytes = alignedSlabSize_;
    stats.totalBytes = totalAllocatedBytes_;
    stats.activeSlabs = activeSlabs_.load(std::memory_order_relaxed);
    stats.peakActiveSlabs = peakActiveSlabs_.load(std::memory_order_relaxed);
    stats.totalAcquisitions = totalAcquisitions_.load(std::memory_order_relaxed);
    stats.exhaustionCount = exhaustionCount_.load(std::memory_order_relaxed);
    stats.isPageLockedMlock = isMlocked_;
    stats.isPopulated = isPopulated_;
    stats.alignmentBytes = kPageAlignment;
    return stats;
}

} // namespace apexcine::l0
