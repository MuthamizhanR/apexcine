#include "apexcine/l0_runtime/ArenaAllocator.hpp"
#include "apexcine/l0_runtime/EdfScheduler.hpp"
#include "apexcine/l0_runtime/FrameClockAndTelemetry.hpp"
#include "apexcine/l1_capture/NdkCameraCaptureEngine.hpp"
#include "apexcine/l2_pipeline/ComputationalRawMerge.hpp"
#include "apexcine/l2_pipeline/LosslessBayerCodec.hpp"
#include "apexcine/l2_pipeline/RawUnpackAndPlanarize.hpp"
#include "apexcine/l2_pipeline/SensorCalibrationAndClarity.hpp"
#include "apexcine/l3_container/ApexRawContainer.hpp"
#include "apexcine/l4_color/AcesColorPipeline.hpp"
#include "apexcine/lp_platform/DeviceCapabilityProbe.hpp"
#include "apexcine/lp_platform/DeviceQuirkDatabase.hpp"
#include "apexcine/lp_platform/StorageBenchmarkAndIoProber.hpp"
#include "apexcine/lp_platform/ThermalAndAdpfManager.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    using namespace apexcine;

    const std::string traceOutputPath =
        (argc > 1) ? argv[1] : "/tmp/apexcine_4k24_perfetto_trace.json";

    constexpr uint32_t kWidth = 4096;
    constexpr uint32_t kHeight = 2160;
    constexpr size_t kTotalPixels = static_cast<size_t>(kWidth) * kHeight;
    constexpr size_t kFrameBytes16 = kTotalPixels * sizeof(uint16_t); // 17.69 MB per frame

    std::cout << "================================================================================\n";
    std::cout << "  ApexCine Camera OS — 4K24 (4096x2160) RAW12 KPI Benchmark & Perfetto Tracer\n";
    std::cout << "================================================================================\n";

    l0::ArenaAllocator arena(16, 20 * 1024 * 1024);
    l0::FrameClockAndTelemetry telemetry;
    lp::DeviceQuirkDatabase quirkDb;
    const auto nord5Profile = quirkDb.matchDevice("OnePlus", "OnePlus Nord 5");

    // Synthesize realistic 4K (4096x2160) 12-bit Bayer scene (smooth cinematic gradient + fine detail + sensor shot noise)
    std::vector<uint16_t> raw16Source(kTotalPixels);
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const uint16_t base = static_cast<uint16_t>(256 + ((x + y) / 4) % 2800);
            const uint16_t noise = static_cast<uint16_t>(((x * 13 + y * 7) & 0x07u));
            raw16Source[static_cast<size_t>(y) * kWidth + x] = base + noise;
        }
    }

    // 1. Measure L2 Unpack & Planarize (4096x2160)
    std::vector<uint16_t> planarStorage(kTotalPixels);
    auto planes = l2::RawUnpackAndPlanarize::bindPlanarBuffer(planarStorage.data(), kWidth, kHeight);

    const int64_t tUnpack0 = l0::FrameClockAndTelemetry::nowMonotonicNs();
    (void)l2::RawUnpackAndPlanarize::unpackToCanonicalPlanes(
        reinterpret_cast<const uint8_t*>(raw16Source.data()),
        kWidth,
        kHeight,
        kWidth * sizeof(uint16_t),
        l0::PixelFormat::RAW16_SENSOR_LE,
        l0::CfaPattern::RGGB,
        planes);
    const int64_t tUnpack1 = l0::FrameClockAndTelemetry::nowMonotonicNs();
    telemetry.recordSpan("L2_UnpackPlanarize_4K", tUnpack0, tUnpack1, 1, 1);

    // 2. Measure L2LosslessBayerCodec across Low-Noise Gradient, ISO 100, ISO 400, and ISO 1600
    const size_t maxCompBytes = l2::LosslessBayerCodec::maxCompressedSize(kTotalPixels);
    std::vector<uint8_t> compBuffer(maxCompBytes);
    l2::LosslessCompressionStats compStats{};

    const int64_t tComp0 = l0::FrameClockAndTelemetry::nowMonotonicNs();
    (void)l2::LosslessBayerCodec::compressPlanarBayer(
        planes, compBuffer.data(), compBuffer.size(), compStats);
    const int64_t tComp1 = l0::FrameClockAndTelemetry::nowMonotonicNs();
    telemetry.recordSpan("L2_LosslessCodec_4K", tComp0, tComp1, 2, 1);

    constexpr size_t kPackedRaw12Bytes = (kTotalPixels * 3) / 2; // 1.5 bytes/px (13.27 MB/frame)
    const double ratioVsPacked12 =
        static_cast<double>(kPackedRaw12Bytes) / static_cast<double>(compStats.compressedBytes);

    // Measure realistic Poisson-Gaussian noise scenes (ISO 100 sigma=2.2, ISO 400 sigma=5.0, ISO 1600 sigma=11.0 ADU)
    auto benchmarkNoisyIso = [&](uint32_t sigmaMask) -> l2::LosslessCompressionStats {
        std::vector<uint16_t> noisyRaw(kTotalPixels);
        uint32_t s = 123456789u + sigmaMask * 97u;
        for (size_t i = 0; i < kTotalPixels; ++i) {
            s = s * 1664525u + 1013904223u;
            const int32_t jitter = static_cast<int32_t>((s >> 16) & sigmaMask) -
                                   static_cast<int32_t>((s >> 24) & sigmaMask);
            const int32_t v = static_cast<int32_t>(raw16Source[i]) + jitter;
            noisyRaw[i] = static_cast<uint16_t>(std::clamp(v, 0, 4095));
        }
        std::vector<uint16_t> noisyPlanar(kTotalPixels);
        auto np = l2::RawUnpackAndPlanarize::bindPlanarBuffer(noisyPlanar.data(), kWidth, kHeight);
        (void)l2::RawUnpackAndPlanarize::unpackToCanonicalPlanes(
            reinterpret_cast<const uint8_t*>(noisyRaw.data()),
            kWidth, kHeight, kWidth * sizeof(uint16_t),
            l0::PixelFormat::RAW16_SENSOR_LE, l0::CfaPattern::RGGB, np);
        l2::LosslessCompressionStats st{};
        (void)l2::LosslessBayerCodec::compressPlanarBayer(np, compBuffer.data(), compBuffer.size(), st);
        return st;
    };
    const auto statsIso100 = benchmarkNoisyIso(0x03u);  // +/- 3 ADU triangular noise
    const auto statsIso400 = benchmarkNoisyIso(0x07u);  // +/- 7 ADU triangular noise
    const auto statsIso1600 = benchmarkNoisyIso(0x0Fu); // +/- 15 ADU triangular noise

    // Measure 4-worker parallel tile encode throughput across big/prime cores
    constexpr int kNumThreads = 4;
    constexpr int kFramesPerThread = 6;
    std::vector<std::vector<uint8_t>> threadOutBufs(kNumThreads, std::vector<uint8_t>(maxCompBytes));
    const auto parStart = std::chrono::steady_clock::now();
    std::vector<std::thread> workers;
    for (int t = 0; t < kNumThreads; ++t) {
        workers.emplace_back([&, t]() {
            l2::LosslessCompressionStats localStats{};
            for (int f = 0; f < kFramesPerThread; ++f) {
                (void)l2::LosslessBayerCodec::compressPlanarBayer(
                    planes, threadOutBufs[t].data(), threadOutBufs[t].size(), localStats);
            }
        });
    }
    for (auto& th : workers) th.join();
    const auto parEnd = std::chrono::steady_clock::now();
    const double parNs =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(parEnd - parStart).count());
    const double totalBytesEncoded =
        static_cast<double>(kFrameBytes16) * kNumThreads * kFramesPerThread;
    const double aggregateEncodeGBps = totalBytesEncoded / parNs;

    // 3. Measure quarter-res preview superpixel debayer + ACEScct + 10-bit TPDF dither
    constexpr uint32_t kPrevW = 1024, kPrevH = 540;
    std::vector<uint16_t> prevPlanarStorage(static_cast<size_t>(kPrevW) * kPrevH * 4, 1024);
    auto prevPlanes =
        l2::RawUnpackAndPlanarize::bindPlanarBuffer(prevPlanarStorage.data(), kPrevW * 2, kPrevH * 2);
    std::vector<uint32_t> packed1010102(static_cast<size_t>(kPrevW) * kPrevH);
    const uint16_t bl[4] = {256, 256, 256, 256};
    const float wb[4] = {2.1f, 1.0f, 1.0f, 1.65f};
    float camToAp1[9]{};
    l4::AcesColorPipeline::computeCameraToAp1Matrix(nord5Profile.colorProfile, 5600.0f, camToAp1);

    const int64_t tPrev0 = l0::FrameClockAndTelemetry::nowMonotonicNs();
    l4::AcesColorPipeline::debayerAndGradeTo10BitAcescct(
        prevPlanes, bl, 4095, wb, camToAp1, packed1010102.data());
    const int64_t tPrev1 = l0::FrameClockAndTelemetry::nowMonotonicNs();
    telemetry.recordSpan("L4_PreviewSuperpixelACEScct", tPrev0, tPrev1, 3, 1);

    // Record latency budget model (host-measured CPU stages + nominal hardware budget constants)
    l0::FrameLatencySample lat{};
    lat.frameSequenceId = 1;
    lat.sensorReadoutNs = static_cast<int64_t>(nord5Profile.rollingShutterReadout4kMs * 1e6f);
    lat.halDeliveryNs = 5'400'000LL;
    lat.unpackPlanarizeNs = tUnpack1 - tUnpack0;
    lat.gpuDebayerScopesNs = tPrev1 - tPrev0;
    lat.compositionDisplayNs = 8'333'333LL;
    lat.losslessCompressNs = tComp1 - tComp0;
    telemetry.recordFrameLatency(lat, 3'900'000LL, compStats.uncompressedBytes, compStats.compressedBytes);

    // Export Perfetto JSON trace
    {
        std::ofstream ofs(traceOutputPath);
        ofs << telemetry.exportPerfettoTraceJson();
    }

    const auto ioReport = lp::StorageBenchmarkAndIoProber::probeAndBenchmarkDirectory("/tmp", 8 * 1024 * 1024);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Device Profile Matched        : " << nord5Profile.marketingName << " ("
              << nord5Profile.primarySensorName << ") [Verified="
              << (nord5Profile.isHardwareVerified ? "YES" : "NO_UNVERIFIED_PROFILE") << "]\n";
    std::cout << "  [Host Measured] 4K Unpack     : " << (tUnpack1 - tUnpack0) / 1e6 << " ms\n";
    std::cout << "  [Host Measured] 4K Codec Ratio:\n";
    std::cout << "    - Clean / Merged Scene      : " << compStats.compressionRatio
              << ":1 vs 16-bit (" << ratioVsPacked12 << ":1 vs packed RAW12) ["
              << (compStats.compressedBytes / 1024) << " KiB]\n";
    std::cout << "    - ISO 100 Noisy Scene       : " << statsIso100.compressionRatio
              << ":1 vs 16-bit (" << (static_cast<double>(kPackedRaw12Bytes) / statsIso100.compressedBytes)
              << ":1 vs packed RAW12)\n";
    std::cout << "    - ISO 400 Noisy Scene       : " << statsIso400.compressionRatio
              << ":1 vs 16-bit (" << (static_cast<double>(kPackedRaw12Bytes) / statsIso400.compressedBytes)
              << ":1 vs packed RAW12)\n";
    std::cout << "    - ISO 1600 Noisy Scene      : " << statsIso1600.compressionRatio
              << ":1 vs 16-bit (" << (static_cast<double>(kPackedRaw12Bytes) / statsIso1600.compressedBytes)
              << ":1 vs packed RAW12)\n";
    std::cout << "  [Host Measured] Encode Speed  : " << aggregateEncodeGBps
              << " GB/s (4 threads on 2-vCPU VM; Target >= 1.50 GB/s)\n";
    std::cout << "  [Host Measured] Preview CPU   : " << (tPrev1 - tPrev0) / 1e6
              << " ms (1024x540 superpixel ACEScct + TPDF dither)\n";
    std::cout << "  [Modeled Budget] Glass2Glass  : " << lat.totalGlassToGlassMs()
              << " ms (combines host CPU stages + nominal 11.2ms readout / 5.4ms HAL / 8.3ms display;\n"
              << "                                   requires physical Android LED-panel test to verify)\n";
    std::cout << "  [Host /tmp] Storage Probe     : " << ioReport.measuredSequentialWriteMBps
              << " MB/s (" << ioReport.diagnosticSummary << ")\n";
    std::cout << "  Perfetto Trace Written To     : " << traceOutputPath << "\n";
    std::cout << "================================================================================\n";
    return 0;
}
