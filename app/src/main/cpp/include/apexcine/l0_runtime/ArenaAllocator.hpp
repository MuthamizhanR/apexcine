#pragma once

#include "apexcine/l0_runtime/BufferContract.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace apexcine::l0 {

struct ArenaStats {
    size_t slabCount{0};
    size_t slabSizeBytes{0};
    size_t totalBytes{0};
    size_t activeSlabs{0};
    size_t peakActiveSlabs{0};
    uint64_t totalAcquisitions{0};
    uint64_t exhaustionCount{0};
    bool isPageLockedMlock{false};
    bool isPopulated{false};
    size_t alignmentBytes{16384}; // 16KB page size alignment for modern Android 15/16
};

/**
 * Layer 0: Pre-allocated, 16KB Page-Aligned, mlock'd Slab Arena Allocator.
 *
 * Guarantees:
 * - Zero malloc/free or mmap calls on the per-frame hot path.
 * - Lock-free O(1) slab acquire & release via atomic bitmask (up to 64 slabs).
 * - Pre-touches every page on init (MAP_POPULATE equivalent) and attempts mlock()
 *   to prevent minor page faults or swap-out during 4K60 RAW takes.
 * - Falls back gracefully to pre-faulted unlocked memory if RLIMIT_MEMLOCK is restricted.
 */
class ArenaAllocator {
public:
    static constexpr size_t kMaxSlabs = 64;
    static constexpr size_t kPageAlignment = 16384; // 16 KB page alignment

    ArenaAllocator(size_t slabCount, size_t slabSizeBytes);
    ~ArenaAllocator() noexcept;

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    /**
     * Lock-free O(1) slab acquisition.
     * Returns true and populates outDescriptor + outPtr if a free slab was claimed.
     */
    bool acquireSlab(ProducerStage producer,
                     ConsumerStage consumer,
                     BufferDescriptor& outDescriptor,
                     uint8_t*& outPtr) noexcept;

    /**
     * Lock-free O(1) slab release by slabIndex.
     */
    bool releaseSlab(uint32_t slabIndex) noexcept;

    /**
     * Direct pointer lookup for a valid slab index.
     */
    uint8_t* getSlabPtr(uint32_t slabIndex) const noexcept;

    ArenaStats getStats() const noexcept;

private:
    size_t slabCount_{0};
    size_t alignedSlabSize_{0};
    size_t totalAllocatedBytes_{0};
    uint8_t* basePtr_{nullptr};
    bool usesMmap_{false};
    bool isMlocked_{false};
    bool isPopulated_{false};

    // 1 bit per slab: 1 = free, 0 = in-use
    alignas(64) std::atomic<uint64_t> freeMask_{0};
    alignas(64) std::atomic<size_t> activeSlabs_{0};
    alignas(64) std::atomic<size_t> peakActiveSlabs_{0};
    alignas(64) std::atomic<uint64_t> totalAcquisitions_{0};
    alignas(64) std::atomic<uint64_t> exhaustionCount_{0};
};

} // namespace apexcine::l0
