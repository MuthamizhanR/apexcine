package com.example.data

import androidx.camera.core.CameraSelector
import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * Unified Pro Camera Modes combining Filmic Pro, MotionCam Pro, GCam, and Open Camera.
 */
enum class ProCameraMode(
    val title: String,
    val subtitle: String,
    val isVideoMode: Boolean
) {
    FILMIC_PRO_VIDEO("FILMIC LOG", "10-Bit Log Video • Pull-Focus • Audio VU", true),
    MOTIONCAM_RAW("MOTION RAW", "ISP-Bypass MCRAW v3 + 200Hz Gyro + DNG16", true),
    GCAM_COMPUTATIONAL("GCAM HDR+", "HDR+ Stacking • Night Sight • Portrait", false),
    OPENCAM_MANUAL("OPEN CAM", "Full Manual • 3-Shot AEB • Level & Grids", false)
}

enum class GridOverlayMode(val label: String) {
    OFF("GRID OFF"),
    RULE_OF_THIRDS("3x3 THIRDS"),
    GOLDEN_RATIO("PHI 1.618"),
    CENTER_CROSSHAIR("CROSSHAIR")
}

enum class FlashTorchMode(val label: String) {
    OFF("FLASH OFF"),
    ON("FLASH ON"),
    TORCH("TORCH ON")
}

enum class SelfTimerMode(val seconds: Int, val label: String) {
    OFF(0, "TIMER OFF"),
    SEC_3(3, "3s TIMER"),
    SEC_10(10, "10s TIMER")
}

enum class AspectRatioGuide(val label: String, val ratioValue: Float) {
    FULL_SENSOR("FULL", 0f),
    WIDESCREEN_16_9("16:9", 16f / 9f),
    ANAMORPHIC_239("2.39:1", 2.39f),
    DCI_185("1.85:1", 1.85f),
    SQUARE_1_1("1:1", 1.0f)
}

enum class ActiveControlDrawer {
    NONE,
    MANUAL_EXPOSURE,
    LOG_AND_LUT,
    ASSIST_AND_GRIDS,
    STABILIZER_GYRO
}

enum class CaptureMode(val label: String, val badge: String, val extension: String) {
    MCRAW_V3_RAW("MCRAW v3 + MP4", "16-BIT BAYER + 200Hz IMU", "mp4"),
    DIRECT_LOG_10BIT("DIRECT LOG 10-BIT", "VULKAN CS -> MP4", "mp4"),
    RAW_PHOTO_DNG("RAW16 DNG + JPG", "UNCOMPRESSED DNG STILL", "jpg")
}

enum class BayerCfaPattern(val code: Int, val label: String) {
    RGGB(0, "RGGB"),
    BGGR(1, "BGGR"),
    GRBG(2, "GRBG"),
    GBRG(3, "GBRG")
}

enum class RawBitDepth(val bits: Int, val label: String, val bytesPerPixel: Float) {
    RAW10_CSI2(10, "RAW10 CSI-2 PACKED", 1.25f),
    RAW12_PACKED(12, "RAW12 MIPI PACKED", 1.5f),
    RAW16_UNCOMPRESSED(16, "RAW16 UNCOMPRESSED", 2.0f)
}

enum class LogTransferProfile(
    val id: Int,
    val shortName: String,
    val fullName: String,
    val dynamicRangeStops: Float,
    val baseIso: Int,
    val colorSpaceTag: String
) {
    ARRI_LOG_C3(0, "LogC3", "ARRI ALEXA LogC3 (AWG)", 14.2f, 800, "ARRI Wide Gamut"),
    ACESCCT(1, "ACEScct", "AMPAS ACEScct (AP1)", 14.6f, 800, "ACES AP1"),
    SONY_SLOG3(2, "S-Log3", "Sony S-Log3 / S-Gamut3.Cine", 14.0f, 800, "S-Gamut3.Cine"),
    DAVINCI_INTERMEDIATE(3, "DWG/DI", "DaVinci Wide Gamut Intermediate", 14.5f, 400, "DaVinci WG"),
    LINEAR_RAW(4, "REC.709", "Natural Camera Rec.709", 13.8f, 100, "Rec.709")
}

enum class ColorMatrixPreset(
    val label: String,
    val description: String,
    val matrix3x3: FloatArray
) {
    ONEPLUS_LYTIA_D65(
        "OnePlus LYT-808 D65",
        "Calibrated dual-layer transistor CMOS -> CIE XYZ -> ACES",
        floatArrayOf(
            1.14f, -0.09f, -0.05f,
            -0.06f, 1.11f, -0.05f,
            -0.02f, -0.12f, 1.14f
        )
    ),
    ARRI_ALEXA_709_MATCH(
        "ARRI ALEV-III Match",
        "Soft highlight roll-off & warm skin-tone vector matrix",
        floatArrayOf(
            1.10f, -0.06f, -0.04f,
            -0.04f, 1.08f, -0.04f,
            -0.02f, -0.09f, 1.11f
        )
    ),
    ACES_AP0_REFERENCE(
        "ACES AP0 Neutral",
        "Strict academy colorimetric scene-referred preservation",
        floatArrayOf(
            1.02f, -0.01f, -0.01f,
            -0.01f, 1.03f, -0.02f,
            -0.01f, -0.02f, 1.03f
        )
    )
}

enum class OisStabilizationMode(val label: String, val detail: String) {
    ONEPLUS_OPTICAL_LOCK("OIS + MESHFLOW 200Hz", "Hardware OIS Lock + 16x16 Spatially-Variant Warp"),
    GYROFLOW_PURE("GYROFLOW QUATERNION", "Pure 200Hz VQF Quaternion + Rolling Shutter Corrector"),
    OFF("RAW SENSOR LOCKED", "Zero Warping — Raw Metadata Recorded to .mcraw")
}

enum class DialParameter(val label: String, val unit: String) {
    SHUTTER_ANGLE("SHUTTER", "°"),
    ISO_SENSITIVITY("ISO", "ISO"),
    WHITE_BALANCE("WB KELVIN", "K"),
    MANUAL_FOCUS("FOCUS", "m"),
    HIGHLIGHT_RECOVERY("HL REC", "%"),
    EV_COMPENSATION("EV COMP", "EV"),
    ZOOM_RATIO("OPTICAL ZOOM", "x")
}

enum class ProResExportCodec(
    val label: String,
    val fourCc: String,
    val containerExt: String,
    val targetMbps: Int,
    val bitDepthLabel: String
) {
    PRORES_422_HQ_10BIT("Apple ProRes 422 HQ", "apch", "mp4", 220, "10-Bit 4:2:2"),
    PRORES_422_LT_10BIT("Apple ProRes 422 LT", "apcs", "mp4", 102, "10-Bit 4:2:2"),
    HEVC_MAIN10_DIRECTLOG("DirectLog HEVC Main10", "hvc1", "mp4", 85, "10-Bit 4:2:0"),
    CINEMA_DNG_SEQUENCE("Uncompressed CinemaDNG", "dng ", "dng", 480, "16-Bit Linear CFA")
}

enum class WorkspaceTab(val title: String, val shortBadge: String) {
    CINE_HUD("CAMERA", "CAM"),
    MEDIA_GALLERY("GALLERY", "GAL"),
    NLE_TRACK("NLE STUDIO", "NLE"),
    GYRO_MESH("GYRO & SYS", "SYS")
}

enum class GalleryFilter(val label: String) {
    ALL("ALL MEDIA"),
    MCRAW_V3("VIDEOS (.MP4)"),
    RAW_DNG("PHOTOS (.JPG/.DNG)"),
    PRORES_MASTER("EXPORTS")
}

data class CameraLensOption(
    val cameraId: String,
    val label: String,
    val lensFacing: Int = CameraSelector.LENS_FACING_BACK,
    val focalLengthMm: Float = 4.7f
)

data class ColorWheelState(
    val puckX: Float = 0f,
    val puckY: Float = 0f,
    val masterWheel: Float = 0f
) {
    fun toRgbOffset(scale: Float): Triple<Float, Float, Float> {
        val r = (puckX * 0.85f + masterWheel) * scale
        val g = (-puckY * 0.75f - puckX * 0.25f + masterWheel) * scale
        val b = (puckY * 0.80f - puckX * 0.35f + masterWheel) * scale
        return Triple(r, g, b)
    }
}

data class NleKeyframe(
    val timeSec: Float,
    val label: String,
    val exposureEv: Float,
    val roiX: Float,
    val roiY: Float
)

data class NleTimelineSegment(
    val segmentId: Long,
    val clipId: Long,
    val title: String,
    val trackLabel: String,
    val inPointSec: Float,
    val outPointSec: Float,
    val totalDurationSec: Float,
    val logProfile: String,
    val keyframes: List<NleKeyframe> = emptyList()
)

data class RenderQueueJob(
    val id: Long,
    val clipTitle: String,
    val codec: ProResExportCodec,
    val progress: Float,
    val isCompleted: Boolean,
    val outputPath: String
)

data class CpuGpuVulkanMetrics(
    val cpuLoadPercent: Int = 18,
    val cpuFrameTimeMs: Float = 2.1f,
    val vulkanComputeDispatchMs: Float = 2.8f,
    val workgroupDimensions: String = "40x23x1 (16x16 WG)",
    val computeBackendName: String = "VULKAN 1.3 / GLES 3.2 10-BIT CS",
    val cameraXAnalyzerFps: Float = 30.0f,
    val ioWriteMBps: Float = 0.0f,
    val thermalCelsius: Float = 34.6f,
    val activeCameraLabel: String = "MAIN 1x",
    val cpuHistory: List<Float> = List(24) { 18f },
    val gpuHistory: List<Float> = List(24) { 2.8f }
)

data class ExposureControls(
    val iso: Int = 400,
    val shutterAngleDeg: Float = 180f,
    val targetFps: Int = 30,
    val kelvin: Int = 5600,
    val tint: Float = 0f,
    val focusDistanceDiopters: Float = 0.0f, // 0.0f = Auto/Continuous AF
    val evCompensation: Float = 0.0f,        // -3.0 EV .. +3.0 EV (OpenCam / GCam)
    val zoomRatio: Float = 1.0f,             // 1.0x .. 8.0x optical/sensor zoom
    val aebBracketEnabled: Boolean = false,  // OpenCam 3-shot (-2EV, 0EV, +2EV) exposure bracketing
    val flashTorchMode: FlashTorchMode = FlashTorchMode.OFF,
    val selfTimerMode: SelfTimerMode = SelfTimerMode.OFF
) {
    val exposureTimeNs: Long
        get() {
            val frameDurationNs = 1_000_000_000L / targetFps.coerceAtLeast(1)
            return ((shutterAngleDeg / 360f) * frameDurationNs).toLong().coerceIn(200_000L, 66_000_000L)
        }

    val shutterFractionText: String
        get() {
            val denom = ((360f / shutterAngleDeg.coerceAtLeast(1f)) * targetFps).toInt().coerceAtLeast(1)
            return "1/$denom"
        }
}

data class GpuIspUniforms(
    val bayerPattern: BayerCfaPattern = BayerCfaPattern.RGGB,
    val bitDepth: RawBitDepth = RawBitDepth.RAW16_UNCOMPRESSED,
    val logProfile: LogTransferProfile = LogTransferProfile.ARRI_LOG_C3,
    val colorMatrixPreset: ColorMatrixPreset = ColorMatrixPreset.ONEPLUS_LYTIA_D65,
    val highlightRecoveryStrength: Float = 0.85f,
    val shadowDetailBoost: Float = 0.25f,
    val hdrPlusToneMapStrength: Float = 0.45f, // GCam HDR+ local contrast & shadow fusion
    val nightSightGain: Float = 0.0f,          // GCam Night Sight low-light amplification
    val portraitBlurStrength: Float = 0.0f,    // GCam Portrait radial depth bokeh
    val rec709MonitorLut: Boolean = true,
    val falseColorEnabled: Boolean = false,
    val focusPeakingEnabled: Boolean = false,
    val zebraEnabled: Boolean = false,
    val zebraThreshold: Float = 0.92f,
    val gridOverlayMode: GridOverlayMode = GridOverlayMode.RULE_OF_THIRDS,
    val aspectRatioGuide: AspectRatioGuide = AspectRatioGuide.FULL_SENSOR,
    val showLiveHistogramPip: Boolean = true,
    val liftR: Float = 0f,
    val liftG: Float = 0f,
    val liftB: Float = 0f,
    val gamma: Float = 1.0f,
    val gain: Float = 1.0f,
    val saturation: Float = 1.0f,
    val contrastPivot: Float = 0.43f,
    val chromaticAberrationFix: Boolean = true,
    val abCompareSplitX: Float = 0f
)

data class GyroQuaternionSample(
    val timestampNs: Long,
    val qW: Float,
    val qX: Float,
    val qY: Float,
    val qZ: Float,
    val pitchDeg: Float,
    val rollDeg: Float,
    val yawDeg: Float,
    val angularVelocityRadSec: Float
)

data class StabilizationState(
    val oisMode: OisStabilizationMode = OisStabilizationMode.ONEPLUS_OPTICAL_LOCK,
    val measuredSampleRateHz: Int = 200,
    val currentSample: GyroQuaternionSample = GyroQuaternionSample(0L, 1f, 0f, 0f, 0f, 0f, 0f, 0f, 0f),
    val rollingShutterReadoutMs: Float = 8.4f,
    val smoothingStrength: Float = 0.72f,
    val horizonLockEnabled: Boolean = true,
    val showMeshGridOverlay: Boolean = false,
    val meshVerticesDx: FloatArray = FloatArray(16 * 16),
    val meshVerticesDy: FloatArray = FloatArray(16 * 16)
)

data class TrackedObjectState(
    val isTrackingActive: Boolean = false,
    val targetLabel: String = "AF/AE Lock ROI",
    val normalizedCenterX: Float = 0.5f,
    val normalizedCenterY: Float = 0.5f,
    val normalizedWidth: Float = 0.22f,
    val normalizedHeight: Float = 0.22f,
    val confidence: Float = 0.98f,
    val velocityX: Float = 0f,
    val velocityY: Float = 0f,
    val autoReframeLocked: Boolean = false,
    val powerWindowColorMaskEnabled: Boolean = false,
    val maskInsideExposureEv: Float = 0.35f,
    val maskOutsideExposureEv: Float = -0.45f,
    val keyframeCount: Int = 0
)

data class ShizukuOptimizerState(
    val shizukuInstalled: Boolean = false,
    val shizukuBinderAlive: Boolean = false,
    val cineBoostEngaged: Boolean = false,
    val adpfThermalStatusLabel: String = "NOMINAL (34.8°C)",
    val junctionTempCelsius: Float = 34.8f,
    val cpuFrameTimeMs: Float = 2.1f,
    val gpuShaderTimeMs: Float = 2.8f,
    val writerQueueUsedSlabs: Int = 0,
    val writerQueueTotalSlabs: Int = 64,
    val sustainedWriteBandwidthMBps: Float = 0.0f,
    val pinRenderThreads: Boolean = true,
    val lockSustainedGpuClocks: Boolean = true,
    val bypassHal3aLoop: Boolean = true,
    val trimCachedBackgroundApps: Boolean = true,
    val freedRamMb: Int = 0,
    val frameTimeHistoryMs: List<Float> = List(40) { 3.2f },
    val lastCommandLog: String = "Hardware Camera2 + Vulkan/GLES Compute Pipeline Active"
)

@Entity(tableName = "cine_clips")
data class CineClipEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val title: String,
    val filePath: String,
    val captureMode: String,
    val resolutionLabel: String,
    val fps: Int,
    val durationSec: Float,
    val averageBandwidthMBps: Float,
    val logProfileName: String,
    val bayerFormatName: String,
    val gyroSamplesCount: Int,
    val rollingShutterMs: Float,
    val liftR: Float = 0f,
    val liftG: Float = 0f,
    val liftB: Float = 0f,
    val gamma: Float = 1.0f,
    val gain: Float = 1.0f,
    val saturation: Float = 1.0f,
    val highlightRecovery: Float = 0.85f,
    val trackedRoiX: Float = 0.5f,
    val trackedRoiY: Float = 0.5f,
    val lastExportPath: String = "",
    val lastExportCodec: String = "",
    val createdAtEpochMs: Long = System.currentTimeMillis()
)

@Entity(tableName = "lut_presets")
data class LutPresetEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val name: String,
    val logProfile: String,
    val liftR: Float,
    val liftG: Float,
    val liftB: Float,
    val gamma: Float,
    val gain: Float,
    val saturation: Float,
    val highlightRecovery: Float
)
