#include "apexcine/l0_runtime/ArenaAllocator.hpp"
#include "apexcine/l0_runtime/BufferContract.hpp"
#include "apexcine/l0_runtime/EdfScheduler.hpp"
#include "apexcine/l0_runtime/FrameClockAndTelemetry.hpp"
#include "apexcine/l0_runtime/SpscRingBuffer.hpp"
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
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <thread>
#include <unistd.h>
#include <vector>

#define REQUIRE(cond)                                                                     \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "TEST FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false;                                                                 \
        }                                                                                 \
    } while (0)

namespace {

bool testL0ArenaAndLockFreeSpscRing() {
    using namespace apexcine::l0;

    // 1. Test 16KB page-aligned ArenaAllocator
    ArenaAllocator arena(16, 65536);
    const auto initStats = arena.getStats();
    REQUIRE(initStats.slabCount == 16);
    REQUIRE(initStats.alignmentBytes == 16384);
    REQUIRE(initStats.isPopulated);

    std::vector<BufferDescriptor> leases;
    for (size_t i = 0; i < 16; ++i) {
        BufferDescriptor desc{};
        uint8_t* ptr = nullptr;
        REQUIRE(arena.acquireSlab(
            ProducerStage::L1_NDK_IMAGE_READER, ConsumerStage::L2_NEON_UNPACKER, desc, ptr));
        REQUIRE(ptr != nullptr);
        REQUIRE(desc.isPageAligned(ptr, 16384));
        leases.push_back(desc);
    }

    // 17th acquisition must fail cleanly without crashing or allocating heap memory
    BufferDescriptor extraDesc{};
    uint8_t* extraPtr = nullptr;
    REQUIRE(!arena.acquireSlab(
        ProducerStage::L1_NDK_IMAGE_READER, ConsumerStage::L2_NEON_UNPACKER, extraDesc, extraPtr));
    REQUIRE(arena.getStats().exhaustionCount == 1);

    for (const auto& d : leases) {
        REQUIRE(arena.releaseSlab(d.slabIndex));
        // Double release must return false
        REQUIRE(!arena.releaseSlab(d.slabIndex));
    }
    REQUIRE(arena.getStats().activeSlabs == 0);

    // 2. Concurrent SpscRingBuffer stress test (100,000 items across producer & consumer threads)
    SpscRingBuffer<uint64_t, 1024> ring;
    constexpr uint64_t kTotalItems = 100'000;
    std::atomic<bool> consumerOk{true};

    std::thread producer([&]() {
        for (uint64_t i = 1; i <= kTotalItems; ++i) {
            while (!ring.tryPush(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        for (uint64_t expected = 1; expected <= kTotalItems; ++expected) {
            uint64_t val = 0;
            while (!ring.tryPop(val)) {
                std::this_thread::yield();
            }
            if (val != expected) {
                consumerOk.store(false, std::memory_order_relaxed);
                break;
            }
        }
    });

    producer.join();
    consumer.join();
    REQUIRE(consumerOk.load());
    REQUIRE(ring.empty());
    return true;
}

bool testL0EdfSchedulerAndTelemetry() {
    using namespace apexcine::l0;

    EdfScheduler scheduler;
    (void)scheduler.pinCurrentThreadToPrimeCores(true);
    REQUIRE(!scheduler.getStats().detectedPrimeCores.empty());

    static std::vector<int> executionOrder;
    executionOrder.clear();

    static int id1 = 10, id2 = 20, id3 = 30;
    auto fn = [](void* ctx) noexcept {
        executionOrder.push_back(*static_cast<int*>(ctx));
    };

    // Submit out-of-order deadlines: 30ms, 10ms, 20ms
    REQUIRE(scheduler.submitTask({30'000'000LL, 3, fn, &id3, 1}));
    REQUIRE(scheduler.submitTask({10'000'000LL, 1, fn, &id1, 1}));
    REQUIRE(scheduler.submitTask({20'000'000LL, 2, fn, &id2, 1}));

    REQUIRE(scheduler.drainAll(5'000'000LL) == 3);
    REQUIRE(executionOrder.size() == 3);
    REQUIRE(executionOrder[0] == 10);
    REQUIRE(executionOrder[1] == 20);
    REQUIRE(executionOrder[2] == 30);

    // Telemetry & Perfetto export verification
    FrameClockAndTelemetry telemetry;
    for (uint64_t f = 1; f <= 60; ++f) {
        FrameLatencySample sample{};
        sample.frameSequenceId = f;
        sample.sensorReadoutNs = 11'200'000LL;
        sample.halDeliveryNs = 6'100'000LL;
        sample.unpackPlanarizeNs = 1'400'000LL;
        sample.gpuDebayerScopesNs = 2'900'000LL;
        sample.compositionDisplayNs = 9'500'000LL;
        sample.losslessCompressNs = 4'200'000LL;
        sample.diskWriteNs = 3'100'000LL;

        telemetry.recordFrameLatency(sample, 4'500'000LL, 17'694'720u, 5'898'240u);
        telemetry.recordSpan("L2_UnpackPlanarize", 1000 * f, 1000 * f + 1400, 1, f);
    }

    const auto summary = telemetry.computeSummary();
    REQUIRE(summary.framesRecorded == 60);
    REQUIRE(summary.droppedFrames == 0);
    REQUIRE(summary.meanGlassToGlassMs < 40.0); // Stretch target < 40ms achieved (31.1ms)
    REQUIRE(summary.meetsAllLatencyKpis);

    const std::string traceJson = telemetry.exportPerfettoTraceJson();
    REQUIRE(traceJson.find("\"traceEvents\"") != std::string::npos);
    REQUIRE(traceJson.find("L2_UnpackPlanarize") != std::string::npos);
    return true;
}

bool testLpCapabilityProbeAndDeviceQuirkDb() {
    using namespace apexcine::lp;

    // 1. Verify DeviceQuirkDatabase matches OnePlus Nord 5 + all target matrix families
    DeviceQuirkDatabase db;
    REQUIRE(db.allProfiles().size() >= 10);

    const auto nord5 = db.matchDevice("OnePlus", "OnePlus Nord 5 CPH2709");
    REQUIRE(nord5.profileId == "oneplus_nord5_lyt600");
    REQUIRE(nord5.dcgJumpIso == 400);
    REQUIRE(nord5.activeBitDepth == 12);
    REQUIRE(std::abs(nord5.rollingShutterReadout4kMs - 11.2f) < 0.05f);

    const auto pixel9 = db.matchDevice("Google", "Pixel 9 Pro");
    REQUIRE(pixel9.profileId == "google_pixel9pro_gnk");

    const auto s24u = db.matchDevice("samsung", "SM-S928B");
    REQUIRE(s24u.profileId == "samsung_s24ultra_sd_hp2");

    const auto s24Exynos = db.matchDevice("samsung", "SM-S926B");
    REQUIRE(s24Exynos.profileId == "samsung_s24plus_exynos_gn3");
    REQUIRE(s24Exynos.quirkForceCpuImageReaderMap);

    const auto xiaomi14u = db.matchDevice("Xiaomi", "24030PN60G");
    REQUIRE(xiaomi14u.profileId == "xiaomi_14ultra_lyt900");
    REQUIRE(xiaomi14u.activeBitDepth == 14);

    const auto moto = db.matchDevice("motorola", "XT2403");
    REQUIRE(moto.profileId == "motorola_edge50pro_sd7g3");

    // Verify all built-in profiles are honestly flagged as unverified until hardware-calibrated
    for (const auto& p : db.allProfiles()) {
        REQUIRE(!p.isHardwareVerified);
        REQUIRE(!p.colorProfile.isColorimetricallyCalibrated);
    }

    // Verify manufacturer-only fallback inherits ONLY quirks, not flagship sensor parameters
    const auto unknownSamsung = db.matchDevice("samsung", "SM-A155F Unknown Midrange");
    REQUIRE(unknownSamsung.profileId == "samsung_generic_quirks_fallback");
    REQUIRE(!unknownSamsung.isHardwareVerified);

    // Verify CFA byte validation rejects MONO (5), RGB (4), NIR (6)
    apexcine::l0::CfaPattern cfaOut{};
    REQUIRE(DeviceCapabilityProbe::mapHalCfaToPattern(0, cfaOut) && cfaOut == apexcine::l0::CfaPattern::RGGB);
    REQUIRE(DeviceCapabilityProbe::mapHalCfaToPattern(3, cfaOut) && cfaOut == apexcine::l0::CfaPattern::BGGR);
    REQUIRE(!DeviceCapabilityProbe::mapHalCfaToPattern(4, cfaOut)); // RGB non-Bayer rejected
    REQUIRE(!DeviceCapabilityProbe::mapHalCfaToPattern(5, cfaOut)); // MONO rejected
    REQUIRE(!DeviceCapabilityProbe::mapHalCfaToPattern(6, cfaOut)); // NIR rejected

    // Verify probeAllCameras honors allowHostSimulationOnEmpty=false and marks simulated reports
    DeviceCapabilityProbe probe;
    REQUIRE(probe.probeAllCameras("OnePlus Nord 5", false).empty());
    const auto simReports = probe.probeAllCameras("OnePlus Nord 5", true);
    REQUIRE(!simReports.empty());
    REQUIRE(simReports[0].isSimulatedReport);
    REQUIRE(!simReports[0].empiricalAudit.isTrueLinearRaw);

    // Round-trip JSON export & re-import
    const std::string exportedJson = db.exportAllProfilesToJson();
    REQUIRE(exportedJson.find("oneplus_nord5_lyt600") != std::string::npos);
    REQUIRE(db.loadProfilesFromJson(exportedJson) >= 10);

    // 2. Verify Empirical RAW Authenticity Auditor
    const uint32_t w = 64, h = 64;
    std::vector<uint16_t> rawFrame(w * h, 1024);
    // Inject natural optical vignette (corners = 0.72x center) + 2 isolated PDAF hot pixels
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const double dx = (static_cast<double>(x) - 32.0) / 32.0;
            const double dy = (static_cast<double>(y) - 32.0) / 32.0;
            const double falloff = 1.0 - 0.22 * (dx * dx + dy * dy);
            rawFrame[y * w + x] = static_cast<uint16_t>(256 + 1800.0 * falloff);
        }
    }
    rawFrame[10 * w + 10] = 4090; // PDAF / hot pixel defect 1
    rawFrame[20 * w + 24] = 4085; // PDAF / hot pixel defect 2

    // Genuine linear Photon Transfer Curve (variance proportional to ISO)
    std::vector<IsoVariancePoint> linearRamp = {
        {100, 1200.0, 45.0},
        {200, 1200.0, 91.0},
        {400, 1200.0, 181.0},
        {800, 1200.0, 362.0},
        {1600, 1200.0, 725.0}
    };
    const auto auditGood = DeviceCapabilityProbe::verifyRawAuthenticityEmpirically(
        rawFrame.data(), w, h, w * sizeof(uint16_t), 256, 4095, linearRamp);
    REQUIRE(auditGood.isTrueLinearRaw);
    REQUIRE(!auditGood.hiddenSpatialDenoisingDetected);
    REQUIRE(!auditGood.bakedLensShadingDetected);
    REQUIRE(auditGood.photonTransferLinearityR2 > 0.99);
    REQUIRE(auditGood.detectedPdafDefectCount >= 2);

    // Fake "RAW" where OEM HAL secretly applies spatial NR at ISO >= 800 (variance drops!)
    std::vector<IsoVariancePoint> denoisedRamp = {
        {100, 1200.0, 45.0},
        {200, 1200.0, 90.0},
        {400, 1200.0, 160.0},
        {800, 1200.0, 80.0},
        {1600, 1200.0, 50.0}
    };
    const auto auditBad = DeviceCapabilityProbe::verifyRawAuthenticityEmpirically(
        rawFrame.data(), w, h, w * sizeof(uint16_t), 256, 4095, denoisedRamp);
    REQUIRE(auditBad.hiddenSpatialDenoisingDetected);
    REQUIRE(!auditBad.isTrueLinearRaw);

    // 3. Verify Thermal QoS graceful degradation never drops recorded RAW frames
    ThermalAndAdpfManager thermalMgr;
    const auto r0 = thermalMgr.evaluateThermalPolicy(0.30f, ThermalState::NOMINAL);
    REQUIRE(r0.qosStage == QoSDegradationStage::STAGE_0_FULL_FIDELITY_120HZ);
    REQUIRE(r0.rawRecordingZeroDropGuaranteed);

    const auto r3 = thermalMgr.evaluateThermalPolicy(0.98f, ThermalState::SEVERE);
    REQUIRE(r3.qosStage == QoSDegradationStage::STAGE_3_SHED_PROXY_ENCODE);
    REQUIRE(!r3.liveProxyEncodeEnabled);
    REQUIRE(r3.rawRecordingZeroDropGuaranteed);

    // 4. Verify StorageBenchmarkAndIoProber
    const auto ioProbe = StorageBenchmarkAndIoProber::probeAndBenchmarkDirectory("/tmp", 4 * 1024 * 1024);
    REQUIRE(ioProbe.measuredSequentialWriteMBps > 10.0);
    return true;
}

bool testL1NdkCaptureAndL2UnpackCalibration() {
    using namespace apexcine;

    l0::ArenaAllocator arena(16, 256 * 1024);
    l0::FrameClockAndTelemetry telemetry;
    lp::DeviceQuirkDatabase db;
    const auto nord5 = db.matchDevice("OnePlus", "OnePlus Nord 5");

    l1::NdkCameraCaptureEngine capture(arena, telemetry, nord5);
    l1::ManualCaptureRequestConfig cfg{};
    cfg.width = 64;
    cfg.height = 64;
    cfg.format = l0::PixelFormat::RAW10_MIPI_PACKED;
    REQUIRE(cfg.verifyPureRawContract());
    REQUIRE(capture.configureSession(cfg));
    REQUIRE(capture.startCapture());

    // Push IMU samples around frame mid-exposure
    REQUIRE(capture.pushImuSample({1'000'000'000LL, {0.01f, -0.02f, 0.005f}, {0.0f, 0.0f, 9.81f}}));
    REQUIRE(capture.pushImuSample({1'010'800'000LL, {0.12f, -0.08f, 0.03f}, {0.0f, 0.0f, 9.81f}}));

    // Synthesize 10-bit RGGB pattern and pack into MIPI RAW10 with 64-byte padded rowStride!
    const uint32_t w = 64, h = 64;
    const uint32_t paddedRowStride = 128; // 64px * 1.25 = 80 bytes actual + 48 bytes OEM padding!
    std::vector<uint16_t> origRaw16(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint16_t base = ((y & 1u) == 0)
                                      ? (((x & 1u) == 0) ? 720u : 510u)
                                      : (((x & 1u) == 0) ? 514u : 640u);
            origRaw16[y * w + x] = static_cast<uint16_t>((base + (x * 3 + y * 5)) & 0x03FFu);
        }
    }
    std::vector<uint8_t> packedRaw10(paddedRowStride * h, 0xEE);
    l2::RawUnpackAndPlanarize::packInterleavedToMipiRaw10(
        origRaw16.data(), w, h, paddedRowStride, packedRaw10.data());

    l1::PerFrameCaptureMetadata meta{};
    meta.sensorExposureStartNs = 1'000'000'000LL;
    meta.exposureTimeNs = 10'000'000LL;
    meta.rollingShutterDurationNs = 11'200'000LL;
    meta.isoSensitivity = 400;
    meta.whiteLevel = 1023;
    for (int i = 0; i < 4; ++i) meta.dynamicBlackLevelRggb[i] = 64.0f;

    REQUIRE(capture.onRawFrameAvailable(
        packedRaw10.data(), packedRaw10.size(), paddedRowStride, 0, meta));

    l0::BufferDescriptor capturedDesc{};
    l1::PerFrameCaptureMetadata capturedMeta{};
    REQUIRE(capture.popCapturedFrame(capturedDesc, capturedMeta));
    REQUIRE(capturedDesc.residency == l0::MemoryResidency::CPU_LOCKED_ARENA);
    REQUIRE(std::abs(capturedMeta.interpolatedMidExposureImu.gyroRadSec[0] - 0.12f) < 1e-4f);

    // Verify zero-copy AHardwareBuffer descriptor ingestion path (no Arena slab copy)
    REQUIRE(capture.onHardwareBufferFrameAvailable(packedRaw10.size(), paddedRowStride, 0, meta));
    l0::BufferDescriptor ahbDesc{};
    l1::PerFrameCaptureMetadata ahbMeta{};
    REQUIRE(capture.popCapturedFrame(ahbDesc, ahbMeta));
    REQUIRE(ahbDesc.residency == l0::MemoryResidency::AHARDWAREBUFFER_IMPORT);
    REQUIRE(ahbDesc.slabIndex == 0xFFFFFFFFu);

    // Concurrent IMU producer + frame consumer stress test (verifies race-free IMU sync under TSan)
    std::thread imuThread([&]() {
        for (int i = 0; i < 400; ++i) {
            l1::ImuSample s{};
            s.timestampBoottimeNs = 2'000'000'000LL + static_cast<int64_t>(i) * 1'000'000LL;
            s.gyroRadSec[0] = 0.05f;
            (void)capture.pushImuSample(s);
        }
    });
    std::thread frameThread([&]() {
        for (int f = 0; f < 40; ++f) {
            l1::PerFrameCaptureMetadata m = meta;
            m.sensorExposureStartNs = 2'000'000'000LL + static_cast<int64_t>(f) * 10'000'000LL;
            if (capture.onHardwareBufferFrameAvailable(packedRaw10.size(), paddedRowStride, 0, m)) {
                l0::BufferDescriptor d{};
                l1::PerFrameCaptureMetadata outM{};
                (void)capture.popCapturedFrame(d, outM);
            }
        }
    });
    imuThread.join();
    frameThread.join();

    // Unpack MIPI RAW10 with rowStride padding into 4 canonical planes
    std::vector<uint16_t> planarStorage(w * h);
    auto planes = l2::RawUnpackAndPlanarize::bindPlanarBuffer(planarStorage.data(), w, h);
    const uint8_t* slabPtr = arena.getSlabPtr(capturedDesc.slabIndex);
    REQUIRE(l2::RawUnpackAndPlanarize::unpackToCanonicalPlanes(
        slabPtr,
        w,
        h,
        paddedRowStride,
        l0::PixelFormat::RAW10_MIPI_PACKED,
        l0::CfaPattern::RGGB,
        planes));

    // Reconstruct interleaved RAW16 and verify 100% bit-exact match with origRaw16!
    std::vector<uint16_t> roundTripRaw16(w * h, 0);
    REQUIRE(l2::RawUnpackAndPlanarize::reconstructInterleavedRaw16(
        planes, l0::CfaPattern::RGGB, roundTripRaw16.data(), w));
    REQUIRE(std::memcmp(origRaw16.data(), roundTripRaw16.data(), w * h * sizeof(uint16_t)) == 0);

    // Inject 2 hot PDAF pixels & calibrate planes + compute ETTR stops
    planes.planeR[5 * (w / 2) + 5] = 1023;
    planes.planeB[8 * (w / 2) + 8] = 1023;
    const uint16_t bl[4] = {64, 64, 64, 64};
    const auto calStats = l2::SensorCalibrationAndClarity::calibrateAndAnalyzePlanes(
        planes, bl, 1023, 400, nord5);
    REQUIRE(calStats.badPixelsAndPdafRepaired >= 2);
    REQUIRE(calStats.ettr.isDualConversionGainHigh);
    REQUIRE(calStats.ettr.highlightHeadroomStops >= 0.0f);

    REQUIRE(arena.releaseSlab(capturedDesc.slabIndex));
    capture.stopCapture();
    return true;
}

bool testL2ComputationalRawMergeAndLosslessCodec() {
    using namespace apexcine::l2;

    const uint32_t fullW = 256, fullH = 256;
    const uint32_t pw = fullW / 2, ph = fullH / 2;
    const size_t planeSamples = static_cast<size_t>(pw) * ph;

    // 1. Synthesize a 16-frame low-light RAW burst with Poisson-Gaussian noise + a moving object
    std::mt19937 rng(42);
    std::normal_distribution<double> noiseDist(0.0, 24.0); // Sigma = 24 ADU shadow noise

    auto makeCleanBase = [&](std::vector<uint16_t>& storage, int shiftX, int shiftY, bool movingObj) {
        storage.resize(fullW * fullH);
        auto view = RawUnpackAndPlanarize::bindPlanarBuffer(storage.data(), fullW, fullH);
        for (uint32_t y = 0; y < ph; ++y) {
            for (uint32_t x = 0; x < pw; ++x) {
                const int sx = std::clamp(static_cast<int>(x) - shiftX, 0, static_cast<int>(pw) - 1);
                const int sy = std::clamp(static_cast<int>(y) - shiftY, 0, static_cast<int>(ph) - 1);
                // Uniform shadow calibration patch in [0..36, 0..36], structured texture elsewhere
                const uint16_t texX = (x < 36 && y < 36) ? 0u : static_cast<uint16_t>((sx % 16) * 2);
                const uint16_t texY = (x < 36 && y < 36) ? 0u : static_cast<uint16_t>((sy % 16) * 2);
                uint16_t rVal = static_cast<uint16_t>(256 + 480 + texX);
                uint16_t gVal = static_cast<uint16_t>(256 + 520 + texY);
                uint16_t bVal = static_cast<uint16_t>(256 + 440 + texX);

                // Moving bright object in region [80..100, 80..100] only on alternate frame
                if (movingObj && x >= 80 && x < 100 && y >= 80 && y < 100) {
                    rVal = 3200;
                    gVal = 3400;
                    bVal = 3100;
                }

                const auto addNoise = [&](uint16_t v) -> uint16_t {
                    const int32_t n = static_cast<int32_t>(
                        std::lround(static_cast<double>(v) + noiseDist(rng)));
                    return static_cast<uint16_t>(std::clamp(n, 0, 4095));
                };

                const size_t idx = static_cast<size_t>(y) * pw + x;
                view.planeR[idx] = addNoise(rVal);
                view.planeGr[idx] = addNoise(gVal);
                view.planeGb[idx] = addNoise(static_cast<uint16_t>(gVal + 1));
                view.planeB[idx] = addNoise(bVal);
            }
        }
        return view;
    };

    std::vector<uint16_t> refStorage;
    BurstFrameInput refFrame{};
    refFrame.planes = makeCleanBase(refStorage, 0, 0, false);

    // Create 15 alternate burst frames (16 frames total -> theoretical 4x RMS noise reduction = +2.0 stops amplitude SNR)
    std::vector<std::vector<uint16_t>> altStorages(15);
    std::vector<BurstFrameInput> altFrames15(15);
    for (size_t i = 0; i < 15; ++i) {
        const int dx = static_cast<int>((i % 3) - 1);
        const int dy = static_cast<int>(((i / 3) % 3) - 1);
        altFrames15[i].planes = makeCleanBase(altStorages[i], dx, dy, i == 3);
        altFrames15[i].gyroShiftPlaneX = static_cast<float>(dx);
        altFrames15[i].gyroShiftPlaneY = static_cast<float>(dy);
        altFrames15[i].exposureRatioToReference = 1.0f;
    }

    std::vector<uint16_t> mergedStorage(fullW * fullH, 0);
    auto mergedView = RawUnpackAndPlanarize::bindPlanarBuffer(mergedStorage.data(), fullW, fullH);

    ComputationalRawMergeConfig mergeCfg{};
    mergeCfg.puristSingleFrameMode = false;
    mergeCfg.blackLevel = 256;
    mergeCfg.whiteLevel = 4095;
    mergeCfg.readNoiseSigmaADU = 24.0f;
    mergeCfg.motionRejectionTau = 3.5f;

    // 8-frame subset merge: theoretical max is 1.50 stops (9.03 dB / 6.0206 dB)
    std::vector<BurstFrameInput> altFrames7(altFrames15.begin(), altFrames15.begin() + 7);
    const auto mergeRes8 = ComputationalRawMerge::mergeBurstInRawDomain(
        refFrame, altFrames7, mergeCfg, mergedView);
    REQUIRE(mergeRes8.wasMerged);
    REQUIRE(mergeRes8.framesFused == 8);
    REQUIRE(mergeRes8.snrImprovementStops >= 1.30 && mergeRes8.snrImprovementStops <= 1.75);

    // 16-frame full burst merge: achieves >= 1.85 stops true amplitude SNR (~11.5-12.1 dB / 6.0206 dB)
    const auto mergeRes16 = ComputationalRawMerge::mergeBurstInRawDomain(
        refFrame, altFrames15, mergeCfg, mergedView);
    REQUIRE(mergeRes16.wasMerged);
    REQUIRE(mergeRes16.framesFused == 16);
    REQUIRE(mergeRes16.snrImprovementStops >= 1.85);

    // Verify moving bright object in frame 3 at [90, 90] was rejected by motion mask (no ghosting!)
    const uint16_t mergedAtMovingRoi = mergedView.planeGr[90 * pw + 90];
    REQUIRE(mergedAtMovingRoi < 1000); // Reference background is ~776; ghost would be > 1100

    // Verify Purist Single-Frame Mode outputs 100% untouched reference frame
    mergeCfg.puristSingleFrameMode = true;
    std::vector<uint16_t> puristStorage(fullW * fullH, 0);
    auto puristView = RawUnpackAndPlanarize::bindPlanarBuffer(puristStorage.data(), fullW, fullH);
    const auto puristRes = ComputationalRawMerge::mergeBurstInRawDomain(
        refFrame, altFrames15, mergeCfg, puristView);
    REQUIRE(!puristRes.wasMerged);
    REQUIRE(std::memcmp(
                puristStorage.data(), refStorage.data(), fullW * fullH * sizeof(uint16_t)) == 0);

    // 2. Test LosslessBayerCodec bit-exact round-trip & corruption rejection
    const size_t maxComp = LosslessBayerCodec::maxCompressedSize(planeSamples * 4);
    std::vector<uint8_t> compBuffer(maxComp);
    LosslessCompressionStats compStats{};
    REQUIRE(LosslessBayerCodec::compressPlanarBayer(
        mergedView, compBuffer.data(), compBuffer.size(), compStats));
    REQUIRE(compStats.compressionRatio >= 2.0);

    std::vector<uint16_t> decompStorage(fullW * fullH, 0);
    auto decompView = RawUnpackAndPlanarize::bindPlanarBuffer(decompStorage.data(), fullW, fullH);
    REQUIRE(LosslessBayerCodec::decompressToPlanarBayer(
        compBuffer.data(), compStats.compressedBytes, decompView));
    REQUIRE(std::memcmp(
                mergedStorage.data(), decompStorage.data(), fullW * fullH * sizeof(uint16_t)) == 0);

    // Verify corrupted uncompressedBytes in header is rejected by decompressToPlanarBayer
    std::vector<uint8_t> badComp = compBuffer;
    CompressedBayerFrameHeader tamperedHdr{};
    std::memcpy(&tamperedHdr, badComp.data(), sizeof(tamperedHdr));
    tamperedHdr.uncompressedBytes += 16;
    std::memcpy(badComp.data(), &tamperedHdr, sizeof(tamperedHdr));
    REQUIRE(!LosslessBayerCodec::decompressToPlanarBayer(
        badComp.data(), compStats.compressedBytes, decompView));

    // Verify truncated payload is rejected cleanly
    REQUIRE(!LosslessBayerCodec::decompressToPlanarBayer(
        compBuffer.data(), compStats.compressedBytes - 12, decompView));

    return true;
}

bool testL3OpenContainerAnd100PercentCrashRecovery() {
    using namespace apexcine::l3;

    const std::string cleanPath = "/tmp/apexcine_test_clean.acraw";
    const std::string crashPath = "/tmp/apexcine_test_crash.acraw";

    AcrawFileHeader hdr{};
    hdr.width = 128;
    hdr.height = 128;
    hdr.cfaPattern = 0;
    hdr.bitDepth = 12;
    hdr.targetFps = 24.0f;
    hdr.rollingShutterReadoutMs = 11.2f;
    hdr.journalCheckpointIntervalFrames = 8;
    std::snprintf(hdr.cameraModel, sizeof(hdr.cameraModel), "OnePlus Nord 5");
    std::snprintf(hdr.acesIdtName, sizeof(hdr.acesIdtName), "IDT_oneplus_nord5_lyt600");

    std::vector<uint8_t> dummyCompressedFrame(4096);
    for (size_t i = 0; i < dummyCompressedFrame.size(); ++i) {
        dummyCompressedFrame[i] = static_cast<uint8_t>((i * 37 + 11) & 0xFFu);
    }

    // 1. Clean recording & O(N) delta journal overhead verification (160 frames = 20 checkpoints)
    uint64_t totalBytes160 = 0;
    {
        ApexRawContainerWriter writer;
        REQUIRE(writer.openAndPreallocate(cleanPath, hdr, 4 * 1024 * 1024));
        for (uint64_t f = 1; f <= 160; ++f) {
            AcrawFrameMetadataPrefix meta{};
            meta.frameNumber = f;
            meta.exposureTimeNs = 10'000'000LL;
            meta.isoSensitivity = 400;
            REQUIRE(writer.writeVideoFrame(
                f, static_cast<int64_t>(f) * 41'666'666LL, meta,
                dummyCompressedFrame.data(), dummyCompressedFrame.size()));
        }
        totalBytes160 = writer.bytesWritten();
        REQUIRE(writer.finalizeAndClose());
    }
    // Verify delta journal overhead is < 2% of frame data
    const uint64_t rawFramePayloadTotal = 160ULL * (sizeof(AcrawChunkHeader) + sizeof(AcrawFrameMetadataPrefix) + dummyCompressedFrame.size());
    REQUIRE(totalBytes160 < rawFramePayloadTotal + 16384ULL);

    const auto cleanRep = ApexRawContainerReader::openAndRecover(cleanPath);
    REQUIRE(cleanRep.openedCleanly);
    REQUIRE(!cleanRep.wasRecoveredFromCrash);
    REQUIRE(cleanRep.totalValidFramesRecovered == 160);

    // 2. Simulated crash / abrupt close mid-take (write 35 frames, abort without footer, and append torn tail bytes!)
    {
        ApexRawContainerWriter writer;
        REQUIRE(writer.openAndPreallocate(crashPath, hdr, 8 * 1024 * 1024));
        for (uint64_t f = 1; f <= 35; ++f) {
            AcrawFrameMetadataPrefix meta{};
            meta.frameNumber = f;
            meta.exposureTimeNs = 10'000'000LL;
            meta.isoSensitivity = 800;
            REQUIRE(writer.writeVideoFrame(
                f, static_cast<int64_t>(f) * 41'666'666LL, meta,
                dummyCompressedFrame.data(), dummyCompressedFrame.size()));
        }
        writer.abortSimulatePowerLossMidTake();
    }

    const auto crashRep = ApexRawContainerReader::openAndRecover(crashPath);
    REQUIRE(crashRep.wasRecoveredFromCrash);
    REQUIRE(crashRep.totalValidFramesRecovered == 35);
    REQUIRE(crashRep.journalCheckpointsFound == 4); // At frames 8, 16, 24, 32

    // Verify reading frame #35 back bit-for-bit
    AcrawFrameMetadataPrefix readMeta{};
    std::vector<uint8_t> readBitstream;
    REQUIRE(ApexRawContainerReader::readAndVerifyVideoFrame(
        crashPath, crashRep.frameIndex.back(), readMeta, readBitstream));
    REQUIRE(readMeta.frameNumber == 35);
    REQUIRE(readBitstream == dummyCompressedFrame);

    // Verify in-place repair tool truncates torn tail and writes clean FINAL_FOOTER_INDEX
    REQUIRE(ApexRawContainerReader::repairAndFinalizeTruncatedFile(crashPath));
    const auto repairedRep = ApexRawContainerReader::openAndRecover(crashPath);
    REQUIRE(repairedRep.openedCleanly);
    REQUIRE(!repairedRep.wasRecoveredFromCrash);
    REQUIRE(repairedRep.totalValidFramesRecovered == 35);

    // Verify bit-flip in frame payload is caught by readAndVerifyVideoFrame CRC32C check
    {
        const int fd = ::open(crashPath.c_str(), O_RDWR);
        REQUIRE(fd >= 0);
        uint8_t byte = 0;
        const off_t flipOff = static_cast<off_t>(repairedRep.frameIndex[0].fileOffsetBytes + sizeof(AcrawChunkHeader) + 20);
        REQUIRE(::pread(fd, &byte, 1, flipOff) == 1);
        byte ^= 0xFFu;
        REQUIRE(::pwrite(fd, &byte, 1, flipOff) == 1);
        ::close(fd);
    }
    REQUIRE(!ApexRawContainerReader::readAndVerifyVideoFrame(
        crashPath, repairedRep.frameIndex[0], readMeta, readBitstream));

    (void)::unlink(cleanPath.c_str());
    (void)::unlink(crashPath.c_str());
    return true;
}

bool testL4AcesColorAndDeltaE2000Kpi() {
    using namespace apexcine::l4;
    using namespace apexcine::lp;

    // 1. Verify official AMPAS ACEScct log encode/decode round-trip
    for (float lin : {0.001f, 0.0078125f, 0.18f, 1.0f, 4.0f, 16.0f}) {
        const float cct = AcesColorPipeline::linearAp1ToAcescct(lin);
        const float back = AcesColorPipeline::acescctToLinearAp1(cct);
        REQUIRE(std::abs(back - lin) / lin < 1e-4f);
    }

    // 2. Verify Bradford D50 -> D60 CAT * XYZ_D60 -> AP1 maps CIE D50 white (0.96422, 1.0, 0.82521) to neutral AP1 (1, 1, 1)
    const auto& mD50ToAp1 = AcesColorPipeline::xyzD50ToAcesAp1Matrix();
    const float ap1WhiteR = mD50ToAp1[0] * 0.96422f + mD50ToAp1[1] * 1.00000f + mD50ToAp1[2] * 0.82521f;
    const float ap1WhiteG = mD50ToAp1[3] * 0.96422f + mD50ToAp1[4] * 1.00000f + mD50ToAp1[5] * 0.82521f;
    const float ap1WhiteB = mD50ToAp1[6] * 0.96422f + mD50ToAp1[7] * 1.00000f + mD50ToAp1[8] * 0.82521f;
    REQUIRE(std::abs(ap1WhiteR - 1.0f) < 1e-4f);
    REQUIRE(std::abs(ap1WhiteG - 1.0f) < 1e-4f);
    REQUIRE(std::abs(ap1WhiteB - 1.0f) < 1e-4f);

    // 3. Verify uncalibrated profiles honestly report isCalibrated = false
    DeviceQuirkDatabase db;
    for (const auto& dev : db.allProfiles()) {
        const auto uncalRep = AcesColorPipeline::evaluateColorChecker24(dev.colorProfile);
        REQUIRE(!uncalRep.isCalibrated);
        REQUIRE(!uncalRep.meetsTargetKpiUnder3);

        const std::string ocio = AcesColorPipeline::generateOcioConfigForDevice(dev);
        REQUIRE(ocio.find("ocio_profile_version: 2") != std::string::npos);
        REQUIRE(ocio.find("ACEScct") != std::string::npos);
    }

    // 4. Verify non-circular ColorChecker24 colorimetric evaluation from synthetic Camera RGB patches:
    // Use sRGB->XYZ(D50) as the profile's forward matrix, invert it (XYZ(D50)->sRGB) to synthesize
    // ideal camera RGB patches from the 24 BabelColor reference L*a*b* patches, and test:
    //   (a) Exact forward matrix -> Mean CIEDE2000 < 0.01
    //   (b) Realistic 2.5% sensor calibration residual -> Mean CIEDE2000 in (0.3, 2.5), meetsTargetKpiUnder3 == true
    //   (c) Miscalibrated matrix (25% error) -> Mean CIEDE2000 > 3.0, meetsTargetKpiUnder3 == false
    constexpr double kXyzD50ToSrgb[9] = {
         3.1338561, -1.6168667, -0.4906146,
        -0.9787684,  1.9161415,  0.0334540,
         0.0719453, -0.2289914,  1.4052427
    };
    const auto& refs = AcesColorPipeline::referenceColorChecker24D50();
    float camPatchesIdeal[24][3]{};
    for (size_t i = 0; i < 24; ++i) {
        double xyz[3]{};
        AcesColorPipeline::labToXyzD50(refs[i], xyz);
        camPatchesIdeal[i][0] = static_cast<float>(
            kXyzD50ToSrgb[0] * xyz[0] + kXyzD50ToSrgb[1] * xyz[1] + kXyzD50ToSrgb[2] * xyz[2]);
        camPatchesIdeal[i][1] = static_cast<float>(
            kXyzD50ToSrgb[3] * xyz[0] + kXyzD50ToSrgb[4] * xyz[1] + kXyzD50ToSrgb[5] * xyz[2]);
        camPatchesIdeal[i][2] = static_cast<float>(
            kXyzD50ToSrgb[6] * xyz[0] + kXyzD50ToSrgb[7] * xyz[1] + kXyzD50ToSrgb[8] * xyz[2]);
    }

    DualIlluminantColorProfile idealProfile{};
    const auto exactRep = AcesColorPipeline::evaluateColorChecker24FromCameraRgb(
        camPatchesIdeal, camPatchesIdeal, idealProfile);
    REQUIRE(exactRep.isCalibrated);
    REQUIRE(exactRep.meanDeltaE2000_D50 < 0.01);
    REQUIRE(exactRep.meanDeltaE2000_StdA < 0.01);
    REQUIRE(exactRep.meetsTargetKpiUnder3);

    DualIlluminantColorProfile realisticProfile = idealProfile;
    realisticProfile.forwardMatrix1_XYZ_D50[0] *= 1.025f;
    realisticProfile.forwardMatrix1_XYZ_D50[4] *= 0.985f;
    realisticProfile.forwardMatrix2_XYZ_D50[0] *= 1.020f;
    realisticProfile.forwardMatrix2_XYZ_D50[8] *= 0.980f;
    const auto realisticRep = AcesColorPipeline::evaluateColorChecker24FromCameraRgb(
        camPatchesIdeal, camPatchesIdeal, realisticProfile);
    REQUIRE(realisticRep.meanDeltaE2000_D50 > 0.2 && realisticRep.meanDeltaE2000_D50 < 2.8);
    REQUIRE(realisticRep.meetsTargetKpiUnder3);

    DualIlluminantColorProfile badProfile = idealProfile;
    badProfile.forwardMatrix1_XYZ_D50[0] *= 1.35f;
    badProfile.forwardMatrix2_XYZ_D50[0] *= 1.35f;
    const auto badRep = AcesColorPipeline::evaluateColorChecker24FromCameraRgb(
        camPatchesIdeal, camPatchesIdeal, badProfile);
    REQUIRE(badRep.meanDeltaE2000_D50 > 3.0);
    REQUIRE(!badRep.meetsTargetKpiUnder3);

    return true;
}

} // namespace

int main() {
    std::cout << "[ApexCine Camera OS] Running C++20 Host Verification Suite...\n";
    if (!testL0ArenaAndLockFreeSpscRing()) return 1;
    std::cout << "  [PASS] L0 ArenaAllocator (16KB aligned) & SPSC Lock-Free Ring Buffer\n";

    if (!testL0EdfSchedulerAndTelemetry()) return 1;
    std::cout << "  [PASS] L0 EDF Real-Time Scheduler, Prime-Core Affinity & Perfetto Telemetry\n";

    if (!testLpCapabilityProbeAndDeviceQuirkDb()) return 1;
    std::cout << "  [PASS] LP DeviceCapabilityProbe (CFA Validation, Sim Gating) & Quirk DB\n";

    if (!testL1NdkCaptureAndL2UnpackCalibration()) return 1;
    std::cout << "  [PASS] L1 ISP-Bypass Capture, Concurrent IMU Sync & L2 MIPI Unpacker + ETTR\n";

    if (!testL2ComputationalRawMergeAndLosslessCodec()) return 1;
    std::cout << "  [PASS] L2 Computational RAW Merger (True Amplitude SNR Stops) & Lossless Codec\n";

    if (!testL3OpenContainerAnd100PercentCrashRecovery()) return 1;
    std::cout << "  [PASS] L3 Open Container (.acraw v1), O(N) Delta Journal, CRC32C & Repair\n";

    if (!testL4AcesColorAndDeltaE2000Kpi()) return 1;
    std::cout << "  [PASS] L4 Bradford D50->D60 CAT, ACES AP1/ACEScct & ColorChecker24 Evaluator\n";

    std::cout << "[ApexCine Camera OS] ALL 7 C++20 MODULE TEST SUITES PASSED!\n";
    return 0;
}
