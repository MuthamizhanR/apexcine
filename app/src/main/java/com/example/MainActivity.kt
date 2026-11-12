package com.example

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.data.WorkspaceTab
import com.example.ui.components.FloatingBottomProCameraHud
import com.example.ui.components.FloatingProCameraTopBar
import com.example.ui.components.GpuIspViewfinder
import com.example.ui.components.LiveWaveformAndParadeScopes
import com.example.ui.screens.GyroAndShizukuSystemDeck
import com.example.ui.screens.MediaBinGalleryDeck
import com.example.ui.screens.NleTrackAndGradeDeck
import com.example.ui.screens.StudioWorkspaceHeader
import com.example.ui.theme.MyApplicationTheme
import com.example.ui.theme.OledBlack
import com.example.viewmodel.CinemaEngineViewModel

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        setContent {
            MyApplicationTheme {
                ApexCineWorkstationScreen()
            }
        }
    }
}

@Composable
fun ApexCineWorkstationScreen(
    viewModel: CinemaEngineViewModel = viewModel()
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current

    val activeTab by viewModel.activeTab.collectAsStateWithLifecycle()
    val proCameraMode by viewModel.proCameraMode.collectAsStateWithLifecycle()
    val activeDrawer by viewModel.activeDrawer.collectAsStateWithLifecycle()
    val countdownSec by viewModel.countdownRemainingSec.collectAsStateWithLifecycle()
    val captureMode by viewModel.captureMode.collectAsStateWithLifecycle()
    val availableLenses by viewModel.availableLenses.collectAsStateWithLifecycle()
    val selectedLens by viewModel.selectedLens.collectAsStateWithLifecycle()
    val exposure by viewModel.exposure.collectAsStateWithLifecycle()
    val uniforms by viewModel.ispUniforms.collectAsStateWithLifecycle()
    val activeDialParam by viewModel.activeDialParam.collectAsStateWithLifecycle()
    val telemetry by viewModel.sensorTelemetry.collectAsStateWithLifecycle()
    val stabilization by viewModel.stabilizationState.collectAsStateWithLifecycle()
    val trackedObject by viewModel.trackedObject.collectAsStateWithLifecycle()
    val optimizerState by viewModel.optimizerState.collectAsStateWithLifecycle()
    val computeMetrics by viewModel.computeMetrics.collectAsStateWithLifecycle()

    val clips by viewModel.clips.collectAsStateWithLifecycle()
    val presets by viewModel.presets.collectAsStateWithLifecycle()
    val timelineSegments by viewModel.timelineSegments.collectAsStateWithLifecycle()
    val renderQueue by viewModel.renderQueue.collectAsStateWithLifecycle()
    val nlePlayheadSec by viewModel.nlePlayheadSec.collectAsStateWithLifecycle()
    val isNlePlaying by viewModel.isNlePlaying.collectAsStateWithLifecycle()
    val liftWheel by viewModel.liftWheel.collectAsStateWithLifecycle()
    val gammaWheel by viewModel.gammaWheel.collectAsStateWithLifecycle()
    val gainWheel by viewModel.gainWheel.collectAsStateWithLifecycle()
    val galleryFilter by viewModel.galleryFilter.collectAsStateWithLifecycle()
    val abCompareEnabled by viewModel.abCompareEnabled.collectAsStateWithLifecycle()

    val selectedClipId by viewModel.selectedClipId.collectAsStateWithLifecycle()
    val selectedExportCodec by viewModel.selectedExportCodec.collectAsStateWithLifecycle()
    val bakeGyroFlow by viewModel.bakeGyroFlowOnExport.collectAsStateWithLifecycle()
    val exportProgress by viewModel.exportProgress.collectAsStateWithLifecycle()
    val statusBanner by viewModel.statusBannerMessage.collectAsStateWithLifecycle()

    var hasCameraPerm by remember {
        mutableStateOf(
            ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) ==
                PackageManager.PERMISSION_GRANTED
        )
    }

    val permissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        val granted = result[Manifest.permission.CAMERA] == true
        hasCameraPerm = granted
        viewModel.bindCameraXIfPermitted(lifecycleOwner, granted)
    }

    val requestCameraPermissions = {
        runCatching {
            permissionLauncher.launch(
                arrayOf(Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO)
            )
        }
        Unit
    }

    LaunchedEffect(Unit) {
        if (hasCameraPerm) {
            viewModel.bindCameraXIfPermitted(lifecycleOwner, true)
        } else {
            requestCameraPermissions()
        }
    }

    LaunchedEffect(hasCameraPerm) {
        if (hasCameraPerm) {
            viewModel.bindCameraXIfPermitted(lifecycleOwner, true)
        }
    }

    BackHandler(enabled = activeTab != WorkspaceTab.CINE_HUD) {
        viewModel.selectTab(WorkspaceTab.CINE_HUD)
    }

    val latestClip = clips.firstOrNull()
    val latestThumb = remember(latestClip?.filePath, clips.size) {
        latestClip?.let { viewModel.getThumbnailForClip(it.filePath) }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(OledBlack)
    ) {
        when (activeTab) {
            WorkspaceTab.CINE_HUD -> {
                // 100% FULL-SCREEN EDGE-TO-EDGE LIVE CAMERA VIEWFINDER + FLOATING TRANSLUCENT HUD
                GpuIspViewfinder(
                    gpuPipeline = viewModel.gpuColorPipeline,
                    uniforms = uniforms,
                    exposure = exposure,
                    stabilization = stabilization,
                    trackedObject = trackedObject,
                    telemetry = telemetry,
                    hasCameraPerm = hasCameraPerm,
                    countdownSec = countdownSec,
                    onRequestCameraPerm = requestCameraPermissions,
                    onTapSubjectRoi = { nx, ny ->
                        viewModel.closeControlDrawer()
                        viewModel.lockSubjectAtNormalizedPoint(nx, ny)
                    },
                    onDragAbSplit = viewModel::setAbSplitPosition,
                    modifier = Modifier.fillMaxSize()
                )

                // Floating Top Pro Camera Bar
                FloatingProCameraTopBar(
                    metrics = computeMetrics,
                    telemetry = telemetry,
                    exposure = exposure,
                    uniforms = uniforms,
                    gyroHz = stabilization.measuredSampleRateHz,
                    statusBanner = statusBanner,
                    hasCameraPerm = hasCameraPerm,
                    onRequestCameraPerm = requestCameraPermissions,
                    onCycleFlashTorch = viewModel::cycleFlashTorchMode,
                    onCycleGridOverlay = viewModel::cycleGridOverlayMode,
                    onCycleSelfTimer = viewModel::cycleSelfTimerMode,
                    onToggleHistogramPip = viewModel::toggleLiveHistogramPip,
                    onSelectTab = viewModel::selectTab,
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .windowInsetsPadding(WindowInsets.safeDrawing)
                )

                // Floating Bottom Pro Camera Console (Pop-up Drawers, Lens/Zoom Bar, Mode Carousel & Shutter Dock)
                FloatingBottomProCameraHud(
                    proCameraMode = proCameraMode,
                    activeDrawer = activeDrawer,
                    captureMode = captureMode,
                    availableLenses = availableLenses,
                    selectedLens = selectedLens,
                    exposure = exposure,
                    uniforms = uniforms,
                    activeDialParam = activeDialParam,
                    isRecording = telemetry.isRecording,
                    recordingElapsedSec = telemetry.recordingElapsedSec,
                    showMeshOverlay = stabilization.showMeshGridOverlay,
                    latestClip = latestClip,
                    latestThumbnail = latestThumb,
                    totalClipsCount = clips.size,
                    onSelectProMode = viewModel::selectProCameraMode,
                    onToggleDrawer = viewModel::toggleControlDrawer,
                    onCloseDrawer = viewModel::closeControlDrawer,
                    onSelectCaptureMode = viewModel::selectCaptureMode,
                    onSelectLens = viewModel::selectCameraLens,
                    onFlipCamera = viewModel::flipFrontBackCamera,
                    onSetZoomRatio = viewModel::setZoomRatio,
                    onSelectDialParam = viewModel::selectDialParameter,
                    onStepDialDelta = viewModel::stepDialValue,
                    onSelectLogProfile = viewModel::setLogProfile,
                    onSelectTargetFps = viewModel::setTargetFps,
                    onToggleRec709Lut = viewModel::toggleRec709Lut,
                    onToggleFalseColor = viewModel::toggleFalseColor,
                    onToggleFocusPeaking = viewModel::toggleFocusPeaking,
                    onToggleZebra = viewModel::toggleZebra,
                    onToggleMeshOverlay = viewModel::toggleMeshGridOverlay,
                    onToggleAebBracket = viewModel::toggleAebBracketing,
                    onCycleAspectRatioGuide = viewModel::cycleAspectRatioGuide,
                    onHdrPlusStrengthChanged = viewModel::setHdrPlusStrength,
                    onNightSightGainChanged = viewModel::setNightSightGain,
                    onPortraitBlurChanged = viewModel::setPortraitBlur,
                    onRecordTrigger = viewModel::onRecordTriggerClicked,
                    onCaptureSnapshotNow = viewModel::captureStillSnapshotNow,
                    onOpenGallery = { viewModel.selectTab(WorkspaceTab.MEDIA_GALLERY) },
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .windowInsetsPadding(WindowInsets.safeDrawing)
                )
            }

            else -> {
                // ADAPTIVE PORTRAIT & LANDSCAPE STUDIO WORKSPACE (GALLERY, NLE EDITOR, GYRO & GOVERNOR)
                BoxWithConstraints(
                    modifier = Modifier
                        .fillMaxSize()
                        .windowInsetsPadding(WindowInsets.safeDrawing)
                        .padding(8.dp)
                ) {
                    val isWideLandscape = maxWidth > maxHeight

                    Column(
                        modifier = Modifier.fillMaxSize(),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        StudioWorkspaceHeader(
                            activeTab = activeTab,
                            onSelectTab = viewModel::selectTab
                        )

                        if (isWideLandscape) {
                            Row(
                                modifier = Modifier.fillMaxSize(),
                                horizontalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                Column(
                                    modifier = Modifier
                                        .weight(0.42f)
                                        .fillMaxHeight(),
                                    verticalArrangement = Arrangement.spacedBy(6.dp)
                                ) {
                                    Box(
                                        modifier = Modifier
                                            .weight(1f)
                                            .fillMaxWidth()
                                            .clip(RoundedCornerShape(12.dp))
                                    ) {
                                        GpuIspViewfinder(
                                            gpuPipeline = viewModel.gpuColorPipeline,
                                            uniforms = uniforms,
                                            exposure = exposure,
                                            stabilization = stabilization,
                                            trackedObject = trackedObject,
                                            telemetry = telemetry,
                                            hasCameraPerm = true,
                                            onTapSubjectRoi = viewModel::lockSubjectAtNormalizedPoint,
                                            onDragAbSplit = viewModel::setAbSplitPosition,
                                            modifier = Modifier.fillMaxSize()
                                        )
                                    }
                                    LiveWaveformAndParadeScopes(
                                        telemetry = telemetry,
                                        uniforms = uniforms
                                    )
                                }

                                Box(
                                    modifier = Modifier
                                        .weight(0.58f)
                                        .fillMaxHeight()
                                ) {
                                    ActiveStudioDeckContent(
                                        activeTab = activeTab,
                                        viewModel = viewModel
                                    )
                                }
                            }
                        } else {
                            // Portrait Phone Layout: Top GPU Graded Monitor + Scopes, Bottom Full-Width Studio Deck
                            Column(
                                modifier = Modifier.fillMaxSize(),
                                verticalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                Box(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .height(210.dp)
                                        .clip(RoundedCornerShape(12.dp))
                                ) {
                                    GpuIspViewfinder(
                                        gpuPipeline = viewModel.gpuColorPipeline,
                                        uniforms = uniforms,
                                        exposure = exposure,
                                        stabilization = stabilization,
                                        trackedObject = trackedObject,
                                        telemetry = telemetry,
                                        hasCameraPerm = true,
                                        onTapSubjectRoi = viewModel::lockSubjectAtNormalizedPoint,
                                        onDragAbSplit = viewModel::setAbSplitPosition,
                                        modifier = Modifier.fillMaxSize()
                                    )
                                }
                                LiveWaveformAndParadeScopes(
                                    telemetry = telemetry,
                                    uniforms = uniforms
                                )
                                Box(
                                    modifier = Modifier
                                        .weight(1f)
                                        .fillMaxWidth()
                                ) {
                                    ActiveStudioDeckContent(
                                        activeTab = activeTab,
                                        viewModel = viewModel
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ActiveStudioDeckContent(
    activeTab: WorkspaceTab,
    viewModel: CinemaEngineViewModel
) {
    val clips by viewModel.clips.collectAsStateWithLifecycle()
    val presets by viewModel.presets.collectAsStateWithLifecycle()
    val timelineSegments by viewModel.timelineSegments.collectAsStateWithLifecycle()
    val renderQueue by viewModel.renderQueue.collectAsStateWithLifecycle()
    val nlePlayheadSec by viewModel.nlePlayheadSec.collectAsStateWithLifecycle()
    val isNlePlaying by viewModel.isNlePlaying.collectAsStateWithLifecycle()
    val liftWheel by viewModel.liftWheel.collectAsStateWithLifecycle()
    val gammaWheel by viewModel.gammaWheel.collectAsStateWithLifecycle()
    val gainWheel by viewModel.gainWheel.collectAsStateWithLifecycle()
    val galleryFilter by viewModel.galleryFilter.collectAsStateWithLifecycle()
    val abCompareEnabled by viewModel.abCompareEnabled.collectAsStateWithLifecycle()
    val selectedClipId by viewModel.selectedClipId.collectAsStateWithLifecycle()
    val uniforms by viewModel.ispUniforms.collectAsStateWithLifecycle()
    val trackedObject by viewModel.trackedObject.collectAsStateWithLifecycle()
    val selectedExportCodec by viewModel.selectedExportCodec.collectAsStateWithLifecycle()
    val bakeGyroFlow by viewModel.bakeGyroFlowOnExport.collectAsStateWithLifecycle()
    val exportProgress by viewModel.exportProgress.collectAsStateWithLifecycle()
    val telemetry by viewModel.sensorTelemetry.collectAsStateWithLifecycle()
    val stabilization by viewModel.stabilizationState.collectAsStateWithLifecycle()
    val optimizerState by viewModel.optimizerState.collectAsStateWithLifecycle()

    when (activeTab) {
        WorkspaceTab.MEDIA_GALLERY -> {
            MediaBinGalleryDeck(
                clips = clips,
                selectedClipId = selectedClipId,
                activeFilter = galleryFilter,
                abCompareEnabled = abCompareEnabled,
                telemetry = telemetry,
                getThumbnailForClip = viewModel::getThumbnailForClip,
                onSelectFilter = viewModel::setGalleryFilter,
                onSelectClip = viewModel::selectClip,
                onToggleAbCompare = viewModel::setAbCompareEnabled,
                onOpenInNle = viewModel::openClipInNleEditor,
                onDeleteClip = viewModel::deleteClipFromGallery,
                onBackToCamera = { viewModel.selectTab(WorkspaceTab.CINE_HUD) }
            )
        }

        WorkspaceTab.NLE_TRACK -> {
            NleTrackAndGradeDeck(
                clips = clips,
                presets = presets,
                timelineSegments = timelineSegments,
                renderQueue = renderQueue,
                playheadSec = nlePlayheadSec,
                isPlaying = isNlePlaying,
                liftWheel = liftWheel,
                gammaWheel = gammaWheel,
                gainWheel = gainWheel,
                selectedClipId = selectedClipId,
                uniforms = uniforms,
                trackedObject = trackedObject,
                selectedExportCodec = selectedExportCodec,
                bakeGyroFlow = bakeGyroFlow,
                exportProgress = exportProgress,
                onPlayheadChanged = viewModel::setNlePlayhead,
                onTogglePlayback = viewModel::toggleNlePlayback,
                onRazorSplit = viewModel::razorSplitAtPlayhead,
                onAddKeyframe = viewModel::addKeyframeAtPlayhead,
                onTrimSegment = viewModel::trimSegmentInOut,
                onDragLiftWheel = viewModel::updateLiftWheel,
                onDragGammaWheel = viewModel::updateGammaWheel,
                onDragGainWheel = viewModel::updateGainWheel,
                onSelectClip = viewModel::selectClip,
                onApplyPreset = viewModel::applyLutPreset,
                onResetGrade = viewModel::resetColorGrade,
                onSelectMatrix = viewModel::setColorMatrixPreset,
                onSelectBayer = viewModel::setBayerPattern,
                onSelectBitDepth = viewModel::setRawBitDepth,
                onToggleTracking = viewModel::toggleObjectTrackingActive,
                onToggleAutoReframe = viewModel::toggleAutoReframeLock,
                onTogglePowerWindow = viewModel::togglePowerWindowMask,
                onSelectExportCodec = viewModel::selectExportCodec,
                onToggleBakeGyro = viewModel::toggleBakeGyroFlowOnExport,
                onExportMaster = viewModel::exportSelectedClipToProRes
            )
        }

        WorkspaceTab.GYRO_MESH -> {
            GyroAndShizukuSystemDeck(
                stabilization = stabilization,
                optimizerState = optimizerState,
                onSelectOisMode = viewModel::setOisMode,
                onRollingShutterChanged = viewModel::setRollingShutterMs,
                onSmoothingChanged = viewModel::setSmoothingStrength,
                onHorizonLockChanged = viewModel::setHorizonLock,
                onRecenterGyro = viewModel::recenterGyro,
                onEngageCineBoost = viewModel::engageShizukuCineBoost,
                onToggleOptions = viewModel::toggleOptimizerSwitch
            )
        }

        WorkspaceTab.CINE_HUD -> Unit
    }
}
