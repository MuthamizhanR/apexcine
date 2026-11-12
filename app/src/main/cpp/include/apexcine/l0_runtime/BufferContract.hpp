#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace apexcine::l0 {

/**
 * Layer 0: Explicit Buffer Contract
 * Every buffer in the Camera OS has a single producer, explicit consumer(s),
 * well-defined pixel format, row/plane stride, alignment invariant, and lifetime owner.
 * Zero-copy by default: moving a BufferLease transfers ownership without copying payload bytes.
 */

enum class ProducerStage : uint8_t {
    UNASSIGNED = 0,
    L1_NDK_IMAGE_READER = 1,
    L2_NEON_UNPACKER = 2,
    L2_COMPUTATIONAL_RAW_MERGER = 3,
    L2_LOSSLESS_COMPRESSOR = 4,
    L2_VULKAN_PREVIEW_DEBAYER = 5,
    L3_CONTAINER_MUXER = 6
};

enum class ConsumerStage : uint8_t {
    UNASSIGNED = 0,
    L2_NEON_UNPACKER = 1,
    L2_VULKAN_COMPUTE_PREVIEW = 2,
    L2_LOSSLESS_COMPRESSOR = 3,
    L3_SEQUENTIAL_DISK_WRITER = 4,
    L5_SURFACE_VIEW_PRESENTER = 5
};

enum class PixelFormat : uint8_t {
    UNKNOWN = 0,
    RAW10_MIPI_PACKED = 1,     // 5 bytes per 4 pixels, rowStride padded
    RAW12_MIPI_PACKED = 2,     // 3 bytes per 2 pixels, rowStride padded
    RAW16_SENSOR_LE = 3,       // 2 bytes per pixel, little-endian uint16_t
    PLANAR_BAYER_16 = 4,       // 4 contiguous planes (R, Gr, Gb, B) of uint16_t (w/2 * h/2 each)
    COMPRESSED_ACRAW_TILE = 5, // Intra-frame lossless compressed Bayer bitstream
    RGBA_1010102_PACKED = 6,   // 10-bit DirectLog / preview output (VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    ACESCCT_RGBA_FP16 = 7      // 16-bit half-float ACEScct working buffer
};

enum class CfaPattern : uint8_t {
    RGGB = 0,
    GRBG = 1,
    GBRG = 2,
    BGGR = 3
};

enum class MemoryResidency : uint8_t {
    CPU_LOCKED_ARENA = 0,       // Pre-allocated, page-aligned (16KB), mlock'd RAM ring slab
    AHARDWAREBUFFER_IMPORT = 1, // Zero-copy NDK AHardwareBuffer shared with Vulkan
    IO_ALIGNED_SLAB = 2         // 4096/16384-aligned slab suitable for O_DIRECT / io_uring / pwritev
};

struct alignas(64) BufferDescriptor {
    uint64_t frameSequenceId{0};
    int64_t sensorTimestampNs{0};      // SENSOR_TIMESTAMP (start of exposure)
    int64_t readoutTimestampNs{0};     // SENSOR_READOUT_TIMESTAMP (API 33+ or estimated)
    int64_t exposureDurationNs{0};
    int64_t rollingShutterReadoutNs{0};
    uint32_t isoSensitivity{100};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t rowStrideBytes{0};
    uint32_t pixelStrideBytes{0};
    uint32_t bitDepth{12};
    uint16_t blackLevel[4]{64, 64, 64, 64};
    uint16_t whiteLevel{4095};
    float wbGainsRggb[4]{2.0f, 1.0f, 1.0f, 1.6f};
    float greenSplit{0.0f};
    PixelFormat format{PixelFormat::UNKNOWN};
    CfaPattern cfa{CfaPattern::RGGB};
    MemoryResidency residency{MemoryResidency::CPU_LOCKED_ARENA};
    ProducerStage producer{ProducerStage::UNASSIGNED};
    ConsumerStage consumer{ConsumerStage::UNASSIGNED};
    uint32_t slabIndex{0xFFFFFFFFu};
    size_t capacityBytes{0};
    size_t validBytes{0};
    uint32_t payloadCrc32c{0};

    constexpr bool isValid() const noexcept {
        return width > 0 && height > 0 && capacityBytes > 0 &&
               rowStrideBytes >= (width * pixelStrideBytes);
    }

    inline bool isPageAligned(const void* ptr, size_t alignment = 4096) const noexcept {
        return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
    }

    constexpr int64_t midExposureTimestampNs() const noexcept {
        return sensorTimestampNs + (exposureDurationNs / 2) + (rollingShutterReadoutNs / 2);
    }
};

} // namespace apexcine::l0
