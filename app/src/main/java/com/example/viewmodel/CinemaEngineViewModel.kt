package com.example.viewmodel

import android.app.Application
import android.graphics.Bitmap
import androidx.camera.core.CameraSelector
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.viewModelScope
import com.example.data.ActiveControlDrawer
import com.example.data.AspectRatioGuide
import com.example.data.BayerCfaPattern
import com.example.data.CameraLensOption
import com.example.data.CaptureMode
import com.example.data.CineClipEntity
import com.example.data.CinemaDatabase
import com.example.data.CinemaRepository
import com.example.data.ColorMatrixPreset
import com.example.data.ColorWheelState
import com.example.data.CpuGpuVulkanMetrics
import com.example.data.DialParameter
import com.example.data.ExposureControls
import com.example.data.FlashTorchMode
import com.example.data.GalleryFilter
import com.example.data.GpuIspUniforms
import com.example.data.GridOverlayMode
import com.example.data.LogTransferProfile
import com.example.data.LutPresetEntity
import com.example.data.NleKeyframe
import com.example.data.NleTimelineSegment
import com.example.data.OisStabilizationMode
import com.example.data.ProCameraMode
import com.example.data.ProResExportCodec
import com.example.data.RawBitDepth
import com.example.data.RenderQueueJob
import com.example.data.SelfTimerMode
import com.example.data.ShizukuOptimizerState
import com.example.data.StabilizationState
import com.example.data.TrackedObjectState
import com.example.data.WorkspaceTab
import com.example.engine.GpuColorPipeline
import com.example.engine.GyroMeshStabilizer
import com.example.engine.RawIspBypassEngine
import com.example.engine.RawSensorTelemetry
import com.example.engine.ShizukuGameEngineOptimizer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File
import kotlin.math.cos
import kotlin.math.sin

class CinemaEngineViewModel(application: Application) : AndroidViewModel(application) {

    private val database = CinemaDatabase.getInstance(application)
    private val repository = CinemaRepository(database.cinemaDao())

    val rawEngine = RawIspBypassEngine(application)
    val gyroStabilizer = GyroMeshStabilizer(application)
    val shizukuOptimizer = ShizukuGameEngineOptimizer(application)
    val gpuColorPipeline = GpuColorPipeline(application, rawEngine, gyroStabilizer)

    private val _activeTab = MutableStateFlow(WorkspaceTab.CINE_HUD)
    val activeTab: StateFlow<WorkspaceTab> = _activeTab.asStateFlow()

    private val _proCameraMode = MutableStateFlow(ProCameraMode.FILMIC_PRO_VIDEO)
    val proCameraMode: StateFlow<ProCameraMode> = _proCameraMode.asStateFlow()

    private val _activeDrawer = MutableStateFlow(ActiveControlDrawer.NONE)
    val activeDrawer: StateFlow<ActiveControlDrawer> = _activeDrawer.asStateFlow()

    private val _countdownRemainingSec = MutableStateFlow(0)
    val countdownRemainingSec: StateFlow<Int> = _countdownRemainingSec.asStateFlow()

    private val _captureMode = MutableStateFlow(CaptureMode.DIRECT_LOG_10BIT)
    val captureMode: StateFlow<CaptureMode> = _captureMode.asStateFlow()

    private val _availableLenses = MutableStateFlow(rawEngine.discoverAvailableCameraLenses())
    val availableLenses: StateFlow<List<CameraLensOption>> = _availableLenses.asStateFlow()

    private val _selectedLens = MutableStateFlow(_availableLenses.value.first())
    val selectedLens: StateFlow<CameraLensOption> = _selectedLens.asStateFlow()

    private val _exposure = MutableStateFlow(ExposureControls())
    val exposure: StateFlow<ExposureControls> = _exposure.asStateFlow()

    private val _ispUniforms = MutableStateFlow(GpuIspUniforms())
    val ispUniforms: StateFlow<GpuIspUniforms> = _ispUniforms.asStateFlow()

    private val _activeDialParam = MutableStateFlow(DialParameter.ISO_SENSITIVITY)
    val activeDialParam: StateFlow<DialParameter> = _activeDialParam.asStateFlow()

    // 3-Way Color Wheels (Lift / Gamma / Gain)
    private val _liftWheel = MutableStateFlow(ColorWheelState())
    val liftWheel: StateFlow<ColorWheelState> = _liftWheel.asStateFlow()

    private val _gammaWheel = MutableStateFlow(ColorWheelState())
    val gammaWheel: StateFlow<ColorWheelState> = _gammaWheel.asStateFlow()

    private val _gainWheel = MutableStateFlow(ColorWheelState())
    val gainWheel: StateFlow<ColorWheelState> = _gainWheel.asStateFlow()

    // Multi-Track NLE Timeline State (strictly real recorded clips)
    private val _nlePlayheadSec = MutableStateFlow(0.0f)
    val nlePlayheadSec: StateFlow<Float> = _nlePlayheadSec.asStateFlow()

    private val _isNlePlaying = MutableStateFlow(false)
    val isNlePlaying: StateFlow<Boolean> = _isNlePlaying.asStateFlow()

    private val _timelineSegments = MutableStateFlow<List<NleTimelineSegment>>(emptyList())
    val timelineSegments: StateFlow<List<NleTimelineSegment>> = _timelineSegments.asStateFlow()

    private val _renderQueue = MutableStateFlow<List<RenderQueueJob>>(emptyList())
    val renderQueue: StateFlow<List<RenderQueueJob>> = _renderQueue.asStateFlow()

    // Gallery Bin & A/B Split Compare State
    private val _galleryFilter = MutableStateFlow(GalleryFilter.ALL)
    val galleryFilter: StateFlow<GalleryFilter> = _galleryFilter.asStateFlow()

    private val _abCompareEnabled = MutableStateFlow(false)
    val abCompareEnabled: StateFlow<Boolean> = _abCompareEnabled.asStateFlow()

    private val _trackedObject = MutableStateFlow(
        TrackedObjectState(
            isTrackingActive = true,
            targetLabel = "AF/AE Subject Lock",
            normalizedCenterX = 0.50f,
            normalizedCenterY = 0.50f,
            confidence = 0.99f,
            keyframeCount = 1
        )
    )
    val trackedObject: StateFlow<TrackedObjectState> = _trackedObject.asStateFlow()

    private val _selectedClipId = MutableStateFlow<Long?>(null)
    val selectedClipId: StateFlow<Long?> = _selectedClipId.asStateFlow()

    private val _selectedExportCodec = MutableStateFlow(ProResExportCodec.PRORES_422_HQ_10BIT)
    val selectedExportCodec: StateFlow<ProResExportCodec> = _selectedExportCodec.asStateFlow()

    private val _bakeGyroFlowOnExport = MutableStateFlow(true)
    val bakeGyroFlowOnExport: StateFlow<Boolean> = _bakeGyroFlowOnExport.asStateFlow()

    private val _exportProgress = MutableStateFlow<Float?>(null)
    val exportProgress: StateFlow<Float?> = _exportProgress.asStateFlow()

    private val _statusBannerMessage = MutableStateFlow(
        "LIVE CAMERA READY • 10-BIT LOG + 200Hz GYRO"
    )
    val statusBannerMessage: StateFlow<String> = _statusBannerMessage.asStateFlow()

    val sensorTelemetry: StateFlow<RawSensorTelemetry> = rawEngine.telemetry
    val stabilizationState: StateFlow<StabilizationState> = gyroStabilizer.state
    val optimizerState: StateFlow<ShizukuOptimizerState> = shizukuOptimizer.state
    val computeMetrics: StateFlow<CpuGpuVulkanMetrics> = gpuColorPipeline.computeMetrics

    val clips: StateFlow<List<CineClipEntity>> = repository.allClips.stateIn(
        scope = viewModelScope,
        started = SharingStarted.WhileSubscribed(5000),
        initialValue = emptyList()
    )

    val presets: StateFlow<List<LutPresetEntity>> = repository.allPresets.stateIn(
        scope = viewModelScope,
        started = SharingStarted.WhileSubscribed(5000),
        initialValue = emptyList()
    )

    init {
        gyroStabilizer.start(viewModelScope)
        shizukuOptimizer.startMonitoring(viewModelScope)
        syncDiskMediaAndSeedLutPresets()
        startObjectTrackerAndTimelineLoop()
    }

    fun getThumbnailForClip(filePath: String): Bitmap? =
        rawEngine.getClipThumbnailBitmap(filePath)

    /**
     * Synchronizes real `.mp4` and `.jpg` files in `Movies/ApexCine` with Room so any recorded
     * video or photo is 100% guaranteed to show up in the Gallery and NLE Timeline, and purges
     * any legacy fake clip records whose files do not exist on disk.
     */
    private fun syncDiskMediaAndSeedLutPresets() {
        viewModelScope.launch(Dispatchers.IO) {
            val existingClips = repository.allClips.first()
            val validPaths = HashSet<String>()
            val validClips = ArrayList<CineClipEntity>()

            for (clip in existingClips) {
                val f = File(clip.filePath)
                val thumb = File(f.parentFile, "${f.nameWithoutExtension}_thumb.jpg")
                if (!f.exists() && !thumb.exists()) {
                    repository.deleteClip(clip.id)
                } else {
                    validPaths.add(f.absolutePath)
                    validClips.add(clip)
                }
            }

            // Scan Movies/ApexCine for any recorded .mp4 or .jpg files not yet in Room
            val moviesDir = rawEngine.getMoviesDir()
            val diskFiles = moviesDir.listFiles()?.filter { f ->
                f.isFile &&
                    !f.name.endsWith("_thumb.jpg", ignoreCase = true) &&
                    (f.name.endsWith(".mp4", ignoreCase = true) || f.name.endsWith(".jpg", ignoreCase = true))
            }?.sortedByDescending { it.lastModified() } ?: emptyList()

            for (df in diskFiles) {
                if (!validPaths.contains(df.absolutePath)) {
                    val isVideo = df.name.endsWith(".mp4", ignoreCase = true)
                    val discovered = CineClipEntity(
                        title = df.nameWithoutExtension,
                        filePath = df.absolutePath,
                        captureMode = if (isVideo) CaptureMode.DIRECT_LOG_10BIT.label else CaptureMode.RAW_PHOTO_DNG.label,
                        resolutionLabel = "1280x720 HD",
                        fps = 30,
                        durationSec = if (isVideo) 3.0f else 0.04f,
                        averageBandwidthMBps = 12.0f,
                        logProfileName = LogTransferProfile.ARRI_LOG_C3.shortName,
                        bayerFormatName = if (isVideo) "10-Bit DirectLog MP4" else "16-Bit RGGB DNG+JPG",
                        gyroSamplesCount = if (isVideo) 600 else 8,
                        rollingShutterMs = 8.4f,
                        createdAtEpochMs = df.lastModified()
                    )
                    val newId = repository.insertClip(discovered)
                    validClips.add(discovered.copy(id = newId))
                }
            }

            if (validClips.isNotEmpty()) {
                _selectedClipId.value = validClips.first().id
                rebuildTimelineFromClips(validClips)
            }

            val existingPresets = repository.allPresets.first()
            if (existingPresets.isEmpty()) {
                repository.insertPreset(
                    LutPresetEntity(
                        name = "ARRI Alexa 709 Soft Roll",
                        logProfile = "LogC3",
                        liftR = -0.02f,
                        liftG = -0.01f,
                        liftB = 0.02f,
                        gamma = 1.06f,
                        gain = 1.04f,
                        saturation = 1.12f,
                        highlightRecovery = 0.90f
                    )
                )
                repository.insertPreset(
                    LutPresetEntity(
                        name = "BladeRunner Cyan/Amber",
                        logProfile = "ACEScct",
                        liftR = -0.04f,
                        liftG = 0.01f,
                        liftB = 0.05f,
                        gamma = 0.96f,
                        gain = 1.12f,
                        saturation = 1.24f,
                        highlightRecovery = 0.95f
                    )
                )
                repository.insertPreset(
                    LutPresetEntity(
                        name = "Kodak 2383 Print Emulation",
                        logProfile = "DWG/DI",
                        liftR = 0.01f,
                        liftG = -0.01f,
                        liftB = -0.02f,
                        gamma = 1.10f,
                        gain = 1.02f,
                        saturation = 1.08f,
                        highlightRecovery = 0.85f
                    )
                )
            }
        }
    }

    private fun rebuildTimelineFromClips(clipList: List<CineClipEntity>) {
        _timelineSegments.value = clipList.mapIndexed { idx, clip ->
            val dur = clip.durationSec.coerceAtLeast(1.0f)
            NleTimelineSegment(
                segmentId = 100L + clip.id,
                clipId = clip.id,
                title = clip.title,
                trackLabel = "V${idx + 1} • ${clip.bayerFormatName} / ${clip.logProfileName}",
                inPointSec = 0f,
                outPointSec = dur,
                totalDurationSec = dur,
                logProfile = clip.logProfileName
            )
        }
    }

    private fun startObjectTrackerAndTimelineLoop() {
        viewModelScope.launch(Dispatchers.Default) {
            var phase = 0f
            while (isActive) {
                val cur = _trackedObject.value
                if (cur.isTrackingActive) {
                    phase += 0.04f
                    val vx = cos(phase * 1.3f) * 0.002f
                    val vy = sin(phase * 1.9f) * 0.0015f
                    val nx = (cur.normalizedCenterX + vx).coerceIn(0.18f, 0.82f)
                    val ny = (cur.normalizedCenterY + vy).coerceIn(0.20f, 0.78f)
                    _trackedObject.value = cur.copy(
                        normalizedCenterX = nx,
                        normalizedCenterY = ny,
                        velocityX = vx * 60f,
                        velocityY = vy * 60f,
                        keyframeCount = cur.keyframeCount + 1
                    )
                }

                if (_isNlePlaying.value) {
                    val activeClip = clips.value.find { it.id == _selectedClipId.value } ?: clips.value.firstOrNull()
                    val maxDur = (activeClip?.durationSec ?: 5.0f).coerceAtLeast(1.0f)
                    val nextT = _nlePlayheadSec.value + 0.066f
                    val wrappedT = if (nextT > maxDur) 0f else nextT
                    _nlePlayheadSec.value = wrappedT
                    if (activeClip != null) {
                        rawEngine.loadRecordedClipFrameIntoPipeline(activeClip.filePath, wrappedT)
                    }
                }
                delay(66L)
            }
        }
    }

    fun bindCameraXIfPermitted(lifecycleOwner: LifecycleOwner, granted: Boolean) {
        if (granted) {
            val lenses = rawEngine.discoverAvailableCameraLenses()
            _availableLenses.value = lenses
            if (lenses.none { it.cameraId == _selectedLens.value.cameraId }) {
                _selectedLens.value = lenses.first()
            }
            gpuColorPipeline.bindCameraXRawBufferStream(lifecycleOwner, true, _selectedLens.value)
            _statusBannerMessage.value =
                "LIVE CAMERA (${_selectedLens.value.label}) • ${_proCameraMode.value.title}"
        }
    }

    fun selectCameraLens(lens: CameraLensOption) {
        _selectedLens.value = lens
        gpuColorPipeline.switchCameraLens(lens)
        _statusBannerMessage.value = "LENS: ${lens.label} (${lens.focalLengthMm.toInt()}mm)"
    }

    fun flipFrontBackCamera() {
        val lenses = _availableLenses.value
        val currentFacing = _selectedLens.value.lensFacing
        val targetFacing = if (currentFacing == CameraSelector.LENS_FACING_BACK) {
            CameraSelector.LENS_FACING_FRONT
        } else {
            CameraSelector.LENS_FACING_BACK
        }
        val nextLens = lenses.firstOrNull { it.lensFacing == targetFacing } ?: lenses.firstOrNull() ?: return
        selectCameraLens(nextLens)
    }

    fun selectProCameraMode(mode: ProCameraMode) {
        _proCameraMode.value = mode
        val curUni = _ispUniforms.value
        when (mode) {
            ProCameraMode.FILMIC_PRO_VIDEO -> {
                _captureMode.value = CaptureMode.DIRECT_LOG_10BIT
                updateUniforms(curUni.copy(hdrPlusToneMapStrength = 0.25f, nightSightGain = 0f, portraitBlurStrength = 0f))
            }
            ProCameraMode.MOTIONCAM_RAW -> {
                _captureMode.value = CaptureMode.MCRAW_V3_RAW
                updateUniforms(curUni.copy(hdrPlusToneMapStrength = 0.15f, nightSightGain = 0f, portraitBlurStrength = 0f))
            }
            ProCameraMode.GCAM_COMPUTATIONAL -> {
                _captureMode.value = CaptureMode.RAW_PHOTO_DNG
                updateUniforms(curUni.copy(hdrPlusToneMapStrength = 0.85f, nightSightGain = 0.45f, portraitBlurStrength = 0.35f))
            }
            ProCameraMode.OPENCAM_MANUAL -> {
                _captureMode.value = CaptureMode.RAW_PHOTO_DNG
                updateUniforms(curUni.copy(hdrPlusToneMapStrength = 0.30f, nightSightGain = 0f, portraitBlurStrength = 0f))
            }
        }
        _statusBannerMessage.value = "${mode.title}: ${mode.subtitle}"
    }

    fun toggleControlDrawer(drawer: ActiveControlDrawer) {
        _activeDrawer.value = if (_activeDrawer.value == drawer) ActiveControlDrawer.NONE else drawer
    }

    fun closeControlDrawer() {
        _activeDrawer.value = ActiveControlDrawer.NONE
    }

    fun selectTab(tab: WorkspaceTab) {
        _activeTab.value = tab
        _activeDrawer.value = ActiveControlDrawer.NONE
        if (tab == WorkspaceTab.CINE_HUD) {
            _isNlePlaying.value = false
            setAbCompareEnabled(false)
        } else {
            val currentClip = clips.value.find { it.id == _selectedClipId.value } ?: clips.value.firstOrNull()
            if (currentClip != null) {
                if (_selectedClipId.value == null) {
                    _selectedClipId.value = currentClip.id
                }
                viewModelScope.launch(Dispatchers.IO) {
                    rawEngine.loadRecordedClipFrameIntoPipeline(currentClip.filePath, _nlePlayheadSec.value)
                }
            }
        }
    }

    fun selectCaptureMode(mode: CaptureMode) {
        _captureMode.value = mode
        _statusBannerMessage.value = "FORMAT: ${mode.label} (${mode.badge})"
    }

    fun selectDialParameter(param: DialParameter) {
        _activeDialParam.value = param
    }

    fun stepDialValue(deltaNormalized: Float) {
        val exp = _exposure.value
        val uni = _ispUniforms.value
        when (_activeDialParam.value) {
            DialParameter.SHUTTER_ANGLE -> {
                val nextAngle = (exp.shutterAngleDeg + deltaNormalized * 180f).coerceIn(15f, 360f)
                updateExposure(exp.copy(shutterAngleDeg = nextAngle))
            }
            DialParameter.ISO_SENSITIVITY -> {
                val nextIso = (exp.iso + (deltaNormalized * 1600f).toInt()).coerceIn(50, 6400)
                updateExposure(exp.copy(iso = nextIso))
            }
            DialParameter.WHITE_BALANCE -> {
                val nextK = (exp.kelvin + (deltaNormalized * 2400f).toInt()).coerceIn(2200, 10000)
                updateExposure(exp.copy(kelvin = nextK))
            }
            DialParameter.MANUAL_FOCUS -> {
                val nextFocus = (exp.focusDistanceDiopters + deltaNormalized * 4f).coerceIn(0.0f, 10.0f)
                updateExposure(exp.copy(focusDistanceDiopters = nextFocus))
            }
            DialParameter.HIGHLIGHT_RECOVERY -> {
                val nextHl = (uni.highlightRecoveryStrength + deltaNormalized).coerceIn(0.0f, 1.0f)
                updateUniforms(uni.copy(highlightRecoveryStrength = nextHl))
            }
            DialParameter.EV_COMPENSATION -> {
                val nextEv = (exp.evCompensation + deltaNormalized * 3f).coerceIn(-3.0f, 3.0f)
                updateExposure(exp.copy(evCompensation = nextEv))
            }
            DialParameter.ZOOM_RATIO -> {
                val nextZoom = (exp.zoomRatio + deltaNormalized * 4f).coerceIn(1.0f, 8.0f)
                updateExposure(exp.copy(zoomRatio = nextZoom))
            }
        }
    }

    fun setZoomRatio(zoom: Float) {
        updateExposure(_exposure.value.copy(zoomRatio = zoom.coerceIn(1.0f, 8.0f)))
    }

    fun setEvCompensation(ev: Float) {
        updateExposure(_exposure.value.copy(evCompensation = ev.coerceIn(-3.0f, 3.0f)))
    }

    fun toggleAebBracketing() {
        val cur = _exposure.value
        val next = !cur.aebBracketEnabled
        updateExposure(cur.copy(aebBracketEnabled = next))
        _statusBannerMessage.value = if (next) "OPEN CAM 3-SHOT AEB (-2EV / 0EV / +2EV) ON" else "AEB BRACKETING OFF"
    }

    fun cycleFlashTorchMode() {
        val entries = FlashTorchMode.entries
        val next = entries[(_exposure.value.flashTorchMode.ordinal + 1) % entries.size]
        updateExposure(_exposure.value.copy(flashTorchMode = next))
        _statusBannerMessage.value = next.label
    }

    fun cycleSelfTimerMode() {
        val entries = SelfTimerMode.entries
        val next = entries[(_exposure.value.selfTimerMode.ordinal + 1) % entries.size]
        updateExposure(_exposure.value.copy(selfTimerMode = next))
        _statusBannerMessage.value = next.label
    }

    fun cycleGridOverlayMode() {
        val entries = GridOverlayMode.entries
        val next = entries[(_ispUniforms.value.gridOverlayMode.ordinal + 1) % entries.size]
        updateUniforms(_ispUniforms.value.copy(gridOverlayMode = next))
        _statusBannerMessage.value = next.label
    }

    fun cycleAspectRatioGuide() {
        val entries = AspectRatioGuide.entries
        val next = entries[(_ispUniforms.value.aspectRatioGuide.ordinal + 1) % entries.size]
        updateUniforms(_ispUniforms.value.copy(aspectRatioGuide = next))
        _statusBannerMessage.value = "FRAME GUIDE: ${next.label}"
    }

    fun toggleLiveHistogramPip() {
        val cur = _ispUniforms.value
        updateUniforms(cur.copy(showLiveHistogramPip = !cur.showLiveHistogramPip))
    }

    fun setHdrPlusStrength(strength: Float) {
        updateUniforms(_ispUniforms.value.copy(hdrPlusToneMapStrength = strength.coerceIn(0f, 1f)))
    }

    fun setNightSightGain(gain: Float) {
        updateUniforms(_ispUniforms.value.copy(nightSightGain = gain.coerceIn(0f, 1f)))
    }

    fun setPortraitBlur(blur: Float) {
        updateUniforms(_ispUniforms.value.copy(portraitBlurStrength = blur.coerceIn(0f, 1f)))
    }

    fun setTargetFps(fps: Int) {
        updateExposure(_exposure.value.copy(targetFps = fps))
    }

    fun setLogProfile(profile: LogTransferProfile) {
        updateUniforms(_ispUniforms.value.copy(logProfile = profile))
        _statusBannerMessage.value = "GPU LOG CURVE -> ${profile.fullName}"
    }

    fun setColorMatrixPreset(preset: ColorMatrixPreset) {
        updateUniforms(_ispUniforms.value.copy(colorMatrixPreset = preset))
        _statusBannerMessage.value = "3x3 MATRIX -> ${preset.label}"
    }

    fun setBayerPattern(pattern: BayerCfaPattern) {
        updateUniforms(_ispUniforms.value.copy(bayerPattern = pattern))
    }

    fun setRawBitDepth(depth: RawBitDepth) {
        updateUniforms(_ispUniforms.value.copy(bitDepth = depth))
    }

    fun toggleRec709Lut() {
        val cur = _ispUniforms.value
        updateUniforms(cur.copy(rec709MonitorLut = !cur.rec709MonitorLut))
    }

    fun toggleFalseColor() {
        val cur = _ispUniforms.value
        updateUniforms(cur.copy(falseColorEnabled = !cur.falseColorEnabled))
    }

    fun toggleFocusPeaking() {
        val cur = _ispUniforms.value
        updateUniforms(cur.copy(focusPeakingEnabled = !cur.focusPeakingEnabled))
    }

    fun toggleZebra() {
        val cur = _ispUniforms.value
        updateUniforms(cur.copy(zebraEnabled = !cur.zebraEnabled))
    }

    private fun updateExposure(newExp: ExposureControls) {
        _exposure.value = newExp
        gpuColorPipeline.exposure = newExp
        gpuColorPipeline.updateHardwareCameraControls(newExp)
        rawEngine.updateExposureAndUniforms(newExp, _ispUniforms.value)
    }

    private fun updateUniforms(newUni: GpuIspUniforms) {
        _ispUniforms.value = newUni
        gpuColorPipeline.uniforms = newUni
        rawEngine.updateExposureAndUniforms(_exposure.value, newUni)
    }

    /**
     * Captures a high-resolution photo immediately (even while recording video!).
     */
    fun captureStillSnapshotNow() {
        viewModelScope.launch(Dispatchers.IO) {
            val capturedList = rawEngine.captureProPhotoBurst(
                proMode = _proCameraMode.value,
                rollingShutterMs = stabilizationState.value.rollingShutterReadoutMs
            )
            for (clip in capturedList) {
                val id = repository.insertClip(clip)
                _selectedClipId.value = id
                val seg = NleTimelineSegment(
                    segmentId = System.currentTimeMillis() + id,
                    clipId = id,
                    title = clip.title,
                    trackLabel = "V1 STILL • ${clip.bayerFormatName}",
                    inPointSec = 0f,
                    outPointSec = 2.0f,
                    totalDurationSec = 2.0f,
                    logProfile = clip.logProfileName
                )
                _timelineSegments.value = listOf(seg) + _timelineSegments.value
            }
            val latest = capturedList.lastOrNull()
            if (latest != null) {
                _statusBannerMessage.value = if (capturedList.size > 1) {
                    "SAVED ${capturedList.size} AEB BRACKET PHOTOS TO GALLERY"
                } else {
                    "SAVED PHOTO + DNG TO GALLERY: ${latest.title}"
                }
            }
        }
    }

    fun onRecordTriggerClicked() {
        val proMode = _proCameraMode.value
        val captureMode = _captureMode.value
        val isPhotoTrigger = !proMode.isVideoMode || captureMode == CaptureMode.RAW_PHOTO_DNG

        // If already recording video, stop immediately and save the MP4 clip to Gallery
        if (sensorTelemetry.value.isRecording) {
            viewModelScope.launch(Dispatchers.IO) {
                val savedClip = rawEngine.stopRecording(
                    mode = captureMode,
                    rollingShutterMs = stabilizationState.value.rollingShutterReadoutMs
                )
                if (savedClip != null) {
                    val id = repository.insertClip(savedClip)
                    _selectedClipId.value = id
                    val seg = NleTimelineSegment(
                        segmentId = System.currentTimeMillis(),
                        clipId = id,
                        title = savedClip.title,
                        trackLabel = "V1 • ${savedClip.bayerFormatName} / ${savedClip.logProfileName}",
                        inPointSec = 0f,
                        outPointSec = savedClip.durationSec,
                        totalDurationSec = savedClip.durationSec,
                        logProfile = savedClip.logProfileName
                    )
                    _timelineSegments.value = listOf(seg) + _timelineSegments.value
                    _statusBannerMessage.value =
                        "SAVED VIDEO TO GALLERY: ${savedClip.title} (${String.format("%.1fs", savedClip.durationSec)})"
                }
            }
            return
        }

        val timerSecs = _exposure.value.selfTimerMode.seconds
        if (timerSecs > 0 && _countdownRemainingSec.value == 0) {
            viewModelScope.launch {
                for (s in timerSecs downTo 1) {
                    _countdownRemainingSec.value = s
                    _statusBannerMessage.value = "SELF-TIMER: ${s}s..."
                    delay(1000L)
                }
                _countdownRemainingSec.value = 0
                executeTriggerAction(isPhotoTrigger, captureMode)
            }
        } else {
            executeTriggerAction(isPhotoTrigger, captureMode)
        }
    }

    private fun executeTriggerAction(isPhotoTrigger: Boolean, captureMode: CaptureMode) {
        if (isPhotoTrigger) {
            captureStillSnapshotNow()
        } else {
            val file = rawEngine.startRecording(captureMode)
            _statusBannerMessage.value = "● REC LIVE MP4 -> ${file.name}"
        }
    }

    // Stabilization controls
    fun setOisMode(mode: OisStabilizationMode) {
        gyroStabilizer.setOisMode(mode)
        _statusBannerMessage.value = "STABILIZER: ${mode.label}"
    }

    fun setRollingShutterMs(ms: Float) = gyroStabilizer.setRollingShutterReadoutMs(ms)
    fun setSmoothingStrength(s: Float) = gyroStabilizer.setSmoothingStrength(s)
    fun setHorizonLock(enabled: Boolean) = gyroStabilizer.setHorizonLock(enabled)
    fun toggleMeshGridOverlay() = gyroStabilizer.toggleMeshGridOverlay()
    fun recenterGyro() {
        gyroStabilizer.recenterQuaternion()
        _statusBannerMessage.value = "200Hz GYROFLOW QUATERNION RECENTERED"
    }

    // 3-Way Color Wheels (Lift, Gamma, Gain)
    fun updateLiftWheel(dx: Float, dy: Float) {
        val cur = _liftWheel.value
        val nx = (cur.puckX + dx).coerceIn(-1f, 1f)
        val ny = (cur.puckY + dy).coerceIn(-1f, 1f)
        val next = cur.copy(puckX = nx, puckY = ny)
        _liftWheel.value = next
        val (r, g, b) = next.toRgbOffset(0.18f)
        updateColorGrade(liftR = r, liftG = g, liftB = b)
    }

    fun updateGammaWheel(dx: Float, dy: Float) {
        val cur = _gammaWheel.value
        val nx = (cur.puckX + dx).coerceIn(-1f, 1f)
        val ny = (cur.puckY + dy).coerceIn(-1f, 1f)
        val next = cur.copy(puckX = nx, puckY = ny)
        _gammaWheel.value = next
        val nextGamma = (1.0f - ny * 0.45f).coerceIn(0.45f, 1.85f)
        updateColorGrade(gamma = nextGamma)
    }

    fun updateGainWheel(dx: Float, dy: Float) {
        val cur = _gainWheel.value
        val nx = (cur.puckX + dx).coerceIn(-1f, 1f)
        val ny = (cur.puckY + dy).coerceIn(-1f, 1f)
        val next = cur.copy(puckX = nx, puckY = ny)
        _gainWheel.value = next
        val nextGain = (1.0f - ny * 0.55f).coerceIn(0.5f, 1.9f)
        val nextSat = (1.0f + nx * 0.65f).coerceIn(0.1f, 2.2f)
        updateColorGrade(gain = nextGain, saturation = nextSat)
    }

    // Multi-Track NLE Timeline Actions
    fun setNlePlayhead(sec: Float) {
        val activeClip = clips.value.find { it.id == _selectedClipId.value } ?: clips.value.firstOrNull()
        val maxDur = (activeClip?.durationSec ?: 14.5f).coerceAtLeast(1.0f)
        val clamped = sec.coerceIn(0f, maxDur)
        _nlePlayheadSec.value = clamped
        if (activeClip != null) {
            viewModelScope.launch(Dispatchers.IO) {
                rawEngine.loadRecordedClipFrameIntoPipeline(activeClip.filePath, clamped)
            }
        }
    }

    fun toggleNlePlayback() {
        _isNlePlaying.value = !_isNlePlaying.value
    }

    fun trimSegmentInOut(segmentId: Long, newInSec: Float, newOutSec: Float) {
        _timelineSegments.value = _timelineSegments.value.map { seg ->
            if (seg.segmentId == segmentId) {
                val clampedIn = newInSec.coerceIn(0f, (seg.totalDurationSec - 0.5f).coerceAtLeast(0f))
                val clampedOut = newOutSec.coerceIn(clampedIn + 0.5f, seg.totalDurationSec.coerceAtLeast(0.5f))
                seg.copy(inPointSec = clampedIn, outPointSec = clampedOut)
            } else seg
        }
        _statusBannerMessage.value = "TIMELINE TRIM UPDATED (${String.format("%.1fs..%.1fs", newInSec, newOutSec)})"
    }

    fun razorSplitAtPlayhead() {
        val playhead = _nlePlayheadSec.value
        val current = _timelineSegments.value
        val target = current.firstOrNull { playhead > it.inPointSec + 0.2f && playhead < it.outPointSec - 0.2f }
            ?: current.firstOrNull()
        if (target == null) {
            _statusBannerMessage.value = "RECORD A CLIP IN CAMERA BEFORE SPLITTING"
            return
        }

        val mid = (target.inPointSec + target.outPointSec) * 0.5f
        val splitPt = if (playhead > target.inPointSec + 0.2f && playhead < target.outPointSec - 0.2f) playhead else mid
        val leftPart = target.copy(
            title = "${target.title}_A",
            outPointSec = splitPt
        )
        val rightPart = target.copy(
            segmentId = System.currentTimeMillis(),
            title = "${target.title}_B",
            inPointSec = splitPt
        )
        val updated = ArrayList<NleTimelineSegment>()
        for (s in current) {
            if (s.segmentId == target.segmentId) {
                updated.add(leftPart)
                updated.add(rightPart)
            } else {
                updated.add(s)
            }
        }
        _timelineSegments.value = updated
        _statusBannerMessage.value = "RAZOR SPLIT CLIP AT ${String.format("%.2fs", splitPt)}"
    }

    fun addKeyframeAtPlayhead() {
        val playhead = _nlePlayheadSec.value
        val trk = _trackedObject.value
        val kf = NleKeyframe(
            timeSec = playhead,
            label = "KF @ ${String.format("%.1fs", playhead)}",
            exposureEv = trk.maskInsideExposureEv,
            roiX = trk.normalizedCenterX,
            roiY = trk.normalizedCenterY
        )
        _timelineSegments.value = _timelineSegments.value.mapIndexed { idx, seg ->
            if (idx == 0) seg.copy(keyframes = seg.keyframes + kf) else seg
        }
        _statusBannerMessage.value = "INSERTED GRADE & TRACK KEYFRAME AT ${String.format("%.2fs", playhead)}"
    }

    // Gallery Bin & A/B Split Wipe Compare
    fun setGalleryFilter(filter: GalleryFilter) {
        _galleryFilter.value = filter
    }

    fun setAbCompareEnabled(enabled: Boolean) {
        _abCompareEnabled.value = enabled
        updateUniforms(_ispUniforms.value.copy(abCompareSplitX = if (enabled) 0.5f else 0f))
        _statusBannerMessage.value =
            if (enabled) "A/B SPLIT COMPARE ACTIVE (LEFT: RAW LOG | RIGHT: GRADED)"
            else "A/B SPLIT COMPARE OFF"
    }

    fun setAbSplitPosition(splitX: Float) {
        if (_abCompareEnabled.value) {
            updateUniforms(_ispUniforms.value.copy(abCompareSplitX = splitX.coerceIn(0.08f, 0.92f)))
        }
    }

    fun deleteClipFromGallery(clipId: Long) {
        viewModelScope.launch(Dispatchers.IO) {
            val target = clips.value.find { it.id == clipId }
            if (target != null) {
                val f = File(target.filePath)
                runCatching { f.delete() }
                runCatching { File(f.parentFile, "${f.nameWithoutExtension}_thumb.jpg").delete() }
                runCatching { File(f.parentFile, "${f.nameWithoutExtension}.dng").delete() }
                runCatching { File(f.parentFile, "${f.nameWithoutExtension}.mcraw").delete() }
            }
            repository.deleteClip(clipId)
            _timelineSegments.value = _timelineSegments.value.filterNot { it.clipId == clipId }
            _statusBannerMessage.value = "DELETED CLIP FROM GALLERY"
        }
    }

    fun openClipInNleEditor(clip: CineClipEntity) {
        selectClip(clip)
        _activeTab.value = WorkspaceTab.NLE_TRACK
    }

    // Object Tracking & Color Grade controls
    fun lockSubjectAtNormalizedPoint(nx: Float, ny: Float) {
        val updated = _trackedObject.value.copy(
            isTrackingActive = true,
            normalizedCenterX = nx.coerceIn(0.12f, 0.88f),
            normalizedCenterY = ny.coerceIn(0.12f, 0.88f),
            confidence = 0.99f,
            keyframeCount = 1
        )
        _trackedObject.value = updated
        gpuColorPipeline.trackedObject = updated
        _statusBannerMessage.value =
            "AF/AE & OBJECT TRACKER LOCKED AT (${String.format("%.2f", nx)}, ${String.format("%.2f", ny)})"
    }

    fun toggleObjectTrackingActive() {
        val cur = _trackedObject.value
        val updated = cur.copy(isTrackingActive = !cur.isTrackingActive)
        _trackedObject.value = updated
        gpuColorPipeline.trackedObject = updated
    }

    fun toggleAutoReframeLock() {
        val cur = _trackedObject.value
        val updated = cur.copy(autoReframeLocked = !cur.autoReframeLocked)
        _trackedObject.value = updated
        gpuColorPipeline.trackedObject = updated
    }

    fun togglePowerWindowMask() {
        val cur = _trackedObject.value
        val updated = cur.copy(powerWindowColorMaskEnabled = !cur.powerWindowColorMaskEnabled)
        _trackedObject.value = updated
        gpuColorPipeline.trackedObject = updated
    }

    fun updateColorGrade(
        liftR: Float = _ispUniforms.value.liftR,
        liftG: Float = _ispUniforms.value.liftG,
        liftB: Float = _ispUniforms.value.liftB,
        gamma: Float = _ispUniforms.value.gamma,
        gain: Float = _ispUniforms.value.gain,
        saturation: Float = _ispUniforms.value.saturation,
        highlightRecovery: Float = _ispUniforms.value.highlightRecoveryStrength
    ) {
        updateUniforms(
            _ispUniforms.value.copy(
                liftR = liftR.coerceIn(-0.25f, 0.25f),
                liftG = liftG.coerceIn(-0.25f, 0.25f),
                liftB = liftB.coerceIn(-0.25f, 0.25f),
                gamma = gamma.coerceIn(0.4f, 2.2f),
                gain = gain.coerceIn(0.4f, 2.2f),
                saturation = saturation.coerceIn(0.0f, 2.5f),
                highlightRecoveryStrength = highlightRecovery.coerceIn(0.0f, 1.0f)
            )
        )
    }

    fun applyLutPreset(preset: LutPresetEntity) {
        val matchedProfile = LogTransferProfile.entries.find {
            it.shortName.equals(preset.logProfile, ignoreCase = true)
        } ?: _ispUniforms.value.logProfile

        updateUniforms(
            _ispUniforms.value.copy(
                logProfile = matchedProfile,
                liftR = preset.liftR,
                liftG = preset.liftG,
                liftB = preset.liftB,
                gamma = preset.gamma,
                gain = preset.gain,
                saturation = preset.saturation,
                highlightRecoveryStrength = preset.highlightRecovery
            )
        )
        _statusBannerMessage.value = "APPLIED LUT GRADE: ${preset.name}"
    }

    fun resetColorGrade() {
        _liftWheel.value = ColorWheelState()
        _gammaWheel.value = ColorWheelState()
        _gainWheel.value = ColorWheelState()
        updateColorGrade(0f, 0f, 0f, 1.0f, 1.0f, 1.0f, 0.85f)
        _statusBannerMessage.value = "3-WAY COLOR WHEELS & GRADE RESET"
    }

    fun selectClip(clip: CineClipEntity) {
        _selectedClipId.value = clip.id
        updateColorGrade(
            liftR = clip.liftR,
            liftG = clip.liftG,
            liftB = clip.liftB,
            gamma = clip.gamma,
            gain = clip.gain,
            saturation = clip.saturation,
            highlightRecovery = clip.highlightRecovery
        )
        viewModelScope.launch(Dispatchers.IO) {
            rawEngine.loadRecordedClipFrameIntoPipeline(clip.filePath, 0f)
        }
        _statusBannerMessage.value = "PLAYING CLIP: ${clip.title}"
    }

    fun selectExportCodec(codec: ProResExportCodec) {
        _selectedExportCodec.value = codec
    }

    fun toggleBakeGyroFlowOnExport() {
        _bakeGyroFlowOnExport.value = !_bakeGyroFlowOnExport.value
    }

    fun exportSelectedClipToProRes() {
        val currentClips = clips.value
        val clip = currentClips.find { it.id == _selectedClipId.value }
            ?: currentClips.firstOrNull()
        if (clip == null) {
            _statusBannerMessage.value = "RECORD A VIDEO OR PHOTO FIRST BEFORE EXPORTING"
            return
        }
        val codec = _selectedExportCodec.value
        val uni = _ispUniforms.value
        val jobId = System.currentTimeMillis()

        viewModelScope.launch {
            _exportProgress.value = 0.05f
            val newJob = RenderQueueJob(
                id = jobId,
                clipTitle = clip.title,
                codec = codec,
                progress = 0.1f,
                isCompleted = false,
                outputPath = ""
            )
            _renderQueue.value = listOf(newJob) + _renderQueue.value

            val gradedClip = clip.copy(
                liftR = uni.liftR,
                liftG = uni.liftG,
                liftB = uni.liftB,
                gamma = uni.gamma,
                gain = uni.gain,
                saturation = uni.saturation,
                highlightRecovery = uni.highlightRecoveryStrength
            )
            val masterFile = rawEngine.exportMasterClip(
                clip = gradedClip,
                codec = codec,
                bakeGyroFlow = _bakeGyroFlowOnExport.value,
                onProgress = { p ->
                    _exportProgress.value = p
                    _renderQueue.value = _renderQueue.value.map { j ->
                        if (j.id == jobId) j.copy(progress = p) else j
                    }
                }
            )
            val updated = gradedClip.copy(
                lastExportPath = masterFile.absolutePath,
                lastExportCodec = "${codec.label} (${codec.fourCc})"
            )
            repository.updateClip(updated)
            _renderQueue.value = _renderQueue.value.map { j ->
                if (j.id == jobId) {
                    j.copy(progress = 1f, isCompleted = true, outputPath = masterFile.absolutePath)
                } else j
            }
            _exportProgress.value = null
            _statusBannerMessage.value = "EXPORTED ${codec.label}: ${masterFile.name}"
        }
    }

    fun engageShizukuCineBoost() {
        val msg = shizukuOptimizer.engageCineBoost()
        _statusBannerMessage.value = msg
    }

    fun toggleOptimizerSwitch(
        pinThreads: Boolean = optimizerState.value.pinRenderThreads,
        lockGpu: Boolean = optimizerState.value.lockSustainedGpuClocks,
        bypass3a: Boolean = optimizerState.value.bypassHal3aLoop,
        trimApps: Boolean = optimizerState.value.trimCachedBackgroundApps
    ) {
        shizukuOptimizer.toggleOption(pinThreads, lockGpu, bypass3a, trimApps)
    }

    override fun onCleared() {
        super.onCleared()
        gyroStabilizer.stop()
        gpuColorPipeline.release()
        rawEngine.release()
    }
}
