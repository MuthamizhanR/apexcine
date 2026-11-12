#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace apexcine::l0 {

/**
 * Wait-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 * - Zero dynamic memory allocation on push/pop.
 * - Zero locks/mutexes.
 * - 64-byte cache-line padding between producer head and consumer tail to eliminate false sharing.
 * - Capacity must be a power of two.
 */
template <typename T, size_t CapacityPowerOfTwo>
class SpscRingBuffer {
    static_assert((CapacityPowerOfTwo >= 2) && ((CapacityPowerOfTwo & (CapacityPowerOfTwo - 1)) == 0),
                  "SpscRingBuffer capacity must be a power of two >= 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscRingBuffer element type T must be trivially copyable for zero-overhead hot path");

public:
    static constexpr size_t kCapacity = CapacityPowerOfTwo;
    static constexpr size_t kMask = CapacityPowerOfTwo - 1;

    SpscRingBuffer() noexcept = default;
    ~SpscRingBuffer() noexcept = default;

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    bool tryPush(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);
        if ((head - tail) >= kCapacity) {
            return false; // Ring full
        }
        storage_[head & kMask] = item;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(T& outItem) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        if (tail == head) {
            return false; // Ring empty
        }
        outItem = storage_[tail & kMask];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    size_t sizeApprox() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return head - tail;
    }

    bool empty() const noexcept {
        return sizeApprox() == 0;
    }

    bool full() const noexcept {
        return sizeApprox() >= kCapacity;
    }

    static constexpr size_t capacity() noexcept {
        return kCapacity;
    }

    float occupancyRatio() const noexcept {
        return static_cast<float>(sizeApprox()) / static_cast<float>(kCapacity);
    }

private:
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::array<T, CapacityPowerOfTwo> storage_{};
};

} // namespace apexcine::l0
