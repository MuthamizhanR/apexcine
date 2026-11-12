package com.example.ui.components

import android.opengl.GLSurfaceView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Equalizer
import androidx.compose.material.icons.filled.FlashAuto
import androidx.compose.material.icons.filled.FlashOff
import androidx.compose.material.icons.filled.FlashOn
import androidx.compose.material.icons.filled.GridOn
import androidx.compose.material.icons.filled.Timer
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.VideoSettings
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.data.AspectRatioGuide
import com.example.data.CpuGpuVulkanMetrics
import com.example.data.ExposureControls
import com.example.data.FlashTorchMode
import com.example.data.GpuIspUniforms
import com.example.data.GridOverlayMode
import com.example.data.SelfTimerMode
import com.example.data.StabilizationState
import com.example.data.TrackedObjectState
import com.example.data.WorkspaceTab
import com.example.engine.GpuColorPipeline
import com.example.engine.RawSensorTelemetry
import com.example.ui.theme.CineAmber
import com.example.ui.theme.CineAmberDim
import com.example.ui.theme.ElevatedSlate
import com.example.ui.theme.HudTextPrimary
import com.example.ui.theme.HudTextSecondary
import com.example.ui.theme.OledBlack
import com.example.ui.theme.RecCrimson
import com.example.ui.theme.ScopeEmerald
import com.example.ui.theme.TacticalBorder
import com.example.ui.theme.TacticalCyan
import com.example.ui.theme.TacticalObsidian
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/**
 * Sleek Translucent Floating Top Camera HUD Bar:
 * Displays live REC timer / FPS / GPU / Lens status on the left, and clean 48dp quick-action icons
 * (Flash/Torch, Grid Overlay, Self-Timer, Live Scopes PiP, and NLE/System Switcher) on the right.
 */
@Composable
fun FloatingProCameraTopBar(
    metrics: CpuGpuVulkanMetrics,
    telemetry: RawSensorTelemetry,
    exposure: ExposureControls,
    uniforms: GpuIspUniforms,
    gyroHz: Int,
    statusBanner: String,
    hasCameraPerm: Boolean,
    onRequestCameraPerm: () -> Unit,
    onCycleFlashTorch: () -> Unit,
    onCycleGridOverlay: () -> Unit,
    onCycleSelfTimer: () -> Unit,
    onToggleHistogramPip: () -> Unit,
    onSelectTab: (WorkspaceTab) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 6.dp)
            .testTag("cpu_gpu_telemetry_bar"),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(OledBlack.copy(alpha = 0.68f), RoundedCornerShape(24.dp))
                .border(1.dp, TacticalBorder.copy(alpha = 0.7f), RoundedCornerShape(24.dp))
                .padding(horizontal = 12.dp, vertical = 6.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Left: Live REC Timer or Camera + GPU Telemetry Pill
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(10.dp)
                        .background(
                            if (telemetry.isRecording) RecCrimson else ScopeEmerald,
                            CircleShape
                        )
                )
                if (telemetry.isRecording) {
                    val totalSecs = telemetry.recordingElapsedSec.toInt()
                    val mins = totalSecs / 60
                    val secs = totalSecs % 60
                    val tenths = ((telemetry.recordingElapsedSec * 10).toInt()) % 10
                    Text(
                        text = String.format("REC %02d:%02d.%d", mins, secs, tenths),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.Bold,
                        color = RecCrimson
                    )
                } else {
                    Text(
                        text = "${metrics.activeCameraLabel} • ${exposure.targetFps}P • ${uniforms.logProfile.shortName}",
                        style = MaterialTheme.typography.labelLarge,
                        fontWeight = FontWeight.Bold,
                        color = CineAmber
                    )
                }

                Text(
                    text = "GPU ${String.format("%.1f", metrics.vulkanComputeDispatchMs)}ms • ${gyroHz}Hz",
                    style = MaterialTheme.typography.labelSmall,
                    color = HudTextSecondary
                )

                val dcgLabel = if (telemetry.isDcgHighGainActive) "HCG" else "LCG"
                Text(
                    text = String.format("ETTR +%.1f ST • %s", telemetry.ettrHeadroomStops, dcgLabel),
                    style = MaterialTheme.typography.labelSmall,
                    fontWeight = FontWeight.Bold,
                    color = if (telemetry.ettrHeadroomStops < 0.4f) RecCrimson else TacticalCyan,
                    modifier = Modifier.testTag("hud_ettr_stops_badge")
                )
            }

            // Right: Clean Pro Camera Quick Action Icon Buttons
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                if (!hasCameraPerm) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(14.dp))
                            .background(CineAmber)
                            .clickable { onRequestCameraPerm() }
                            .padding(horizontal = 10.dp, vertical = 4.dp)
                            .testTag("grant_camera_perm_chip")
                    ) {
                        Text(
                            text = "ALLOW CAMERA",
                            style = MaterialTheme.typography.labelSmall,
                            fontWeight = FontWeight.Bold,
                            color = OledBlack
                        )
                    }
                }

                // 1. Flash / Torch Toggle (OpenCam / GCam)
                TopHudIconPill(
                    icon = when (exposure.flashTorchMode) {
                        FlashTorchMode.OFF -> Icons.Filled.FlashOff
                        FlashTorchMode.ON -> Icons.Filled.FlashAuto
                        FlashTorchMode.TORCH -> Icons.Filled.FlashOn
                    },
                    active = exposure.flashTorchMode != FlashTorchMode.OFF,
                    contentDesc = exposure.flashTorchMode.label,
                    tag = "top_btn_flash_torch",
                    onClick = onCycleFlashTorch
                )

                // 2. Grid Overlay Toggle (3x3 / Phi / Crosshair)
                TopHudIconPill(
                    icon = Icons.Filled.GridOn,
                    active = uniforms.gridOverlayMode != GridOverlayMode.OFF,
                    contentDesc = uniforms.gridOverlayMode.label,
                    tag = "top_btn_grid",
                    onClick = onCycleGridOverlay
                )

                // 3. Self-Timer Toggle (Off / 3s / 10s)
                TopHudIconPill(
                    icon = Icons.Filled.Timer,
                    active = exposure.selfTimerMode != SelfTimerMode.OFF,
                    badge = if (exposure.selfTimerMode != SelfTimerMode.OFF) "${exposure.selfTimerMode.seconds}s" else null,
                    contentDesc = exposure.selfTimerMode.label,
                    tag = "top_btn_timer",
                    onClick = onCycleSelfTimer
                )

                // 4. Live Histogram & Audio VU Meter PiP Toggle
                TopHudIconPill(
                    icon = Icons.Filled.Equalizer,
                    active = uniforms.showLiveHistogramPip,
                    contentDesc = "Toggle Live Scopes",
                    tag = "top_btn_scopes_pip",
                    onClick = onToggleHistogramPip
                )

                // 5. Open NLE Colorist Studio
                TopHudIconPill(
                    icon = Icons.Filled.Tune,
                    active = false,
                    contentDesc = "NLE Editor",
                    tag = "nav_tab_nle_track",
                    onClick = { onSelectTab(WorkspaceTab.NLE_TRACK) }
                )

                // 6. Open GyroFlow & System Optimizer
                TopHudIconPill(
                    icon = Icons.Filled.VideoSettings,
                    active = false,
                    contentDesc = "Gyro & System",
                    tag = "nav_tab_gyro_mesh",
                    onClick = { onSelectTab(WorkspaceTab.GYRO_MESH) }
                )
            }
        }

        // Subtle floating status pill + hidden/accessible testTag for status assertions
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .background(OledBlack.copy(alpha = 0.58f), RoundedCornerShape(12.dp))
                    .padding(horizontal = 10.dp, vertical = 3.dp)
            ) {
                Text(
                    text = statusBanner,
                    style = MaterialTheme.typography.labelSmall,
                    color = HudTextPrimary,
                    maxLines = 1,
                    modifier = Modifier.testTag("hud_status_banner")
                )
            }

            // Floating compact Live Scopes + Stereo Audio VU Meter PiP
            if (uniforms.showLiveHistogramPip) {
                FloatingMiniScopesAndVuPip(
                    telemetry = telemetry,
                    uniforms = uniforms
                )
            }
        }
    }
}

@Composable
private fun TopHudIconPill(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    active: Boolean,
    contentDesc: String,
    tag: String,
    badge: String? = null,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .height(34.dp)
            .clip(RoundedCornerShape(17.dp))
            .background(if (active) CineAmber else ElevatedSlate.copy(alpha = 0.85f))
            .clickable { onClick() }
            .padding(horizontal = 9.dp)
            .testTag(tag),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(3.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = contentDesc,
            tint = if (active) OledBlack else HudTextPrimary,
            modifier = Modifier.size(16.dp)
        )
        if (badge != null) {
            Text(
                text = badge,
                style = MaterialTheme.typography.labelSmall,
                fontWeight = FontWeight.Bold,
                color = if (active) OledBlack else CineAmber
            )
        }
    }
}

/**
 * Compact translucent Picture-in-Picture (PiP) Histogram + Luma Waveform + Filmic Pro Stereo Audio VU Meter.
 */
@Composable
fun FloatingMiniScopesAndVuPip(
    telemetry: RawSensorTelemetry,
    uniforms: GpuIspUniforms,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .background(OledBlack.copy(alpha = 0.68f), RoundedCornerShape(10.dp))
            .border(1.dp, TacticalBorder.copy(alpha = 0.7f), RoundedCornerShape(10.dp))
            .padding(horizontal = 8.dp, vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // 1. Mini Luma + RGB Waveform
        Canvas(
            modifier = Modifier
                .width(76.dp)
                .height(26.dp)
        ) {
            val bins = telemetry.waveformLuma
            val colW = size.width / bins.size.coerceAtLeast(1)
            for (i in bins.indices step 2) {
                val v = bins[i].coerceIn(0.08f, 1.0f)
                val barH = v * size.height
                drawLine(
                    color = ScopeEmerald.copy(alpha = 0.9f),
                    start = Offset(i * colW, size.height),
                    end = Offset(i * colW, size.height - barH),
                    strokeWidth = 2.2f
                )
            }
        }

        // 2. Filmic Pro Dual-Channel Stereo Audio dB VU Meter (L / R)
        Column(
            modifier = Modifier.width(62.dp),
            verticalArrangement = Arrangement.spacedBy(3.dp)
        ) {
            val lNorm = ((telemetry.audioLeftDb + 48f) / 48f).coerceIn(0.08f, 1f)
            val rNorm = ((telemetry.audioRightDb + 48f) / 48f).coerceIn(0.08f, 1f)
            AudioVuBar(channel = "L", level = lNorm)
            AudioVuBar(channel = "R", level = rNorm)
        }
    }
}

@Composable
private fun AudioVuBar(channel: String, level: Float) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        Text(
            text = channel,
            style = MaterialTheme.typography.labelSmall,
            color = HudTextSecondary
        )
        Box(
            modifier = Modifier
                .weight(1f)
                .height(5.dp)
                .clip(RoundedCornerShape(3.dp))
                .background(ElevatedSlate)
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth(level)
                    .height(5.dp)
                    .background(
                        when {
                            level > 0.85f -> RecCrimson
                            level > 0.65f -> CineAmber
                            else -> ScopeEmerald
                        }
                    )
            )
        }
    }
}

/**
 * 100% Full-Screen Edge-to-Edge GPU Camera Viewfinder powered by `GpuColorPipeline`.
 * Features Open Camera / GCam / Filmic Pro / MotionCam overlays:
 * - Rule of Thirds / Golden Ratio / Crosshair grids
 * - 2.39:1 / 16:9 / 1.85:1 / 1:1 Cinema Frame Guides
 * - 200Hz GyroFlow Horizon Level (turns green when level)
 * - Tap-to-Focus & Subject Tracking Reticle
 * - Self-Timer Countdown Overlay
 */
@Composable
fun GpuIspViewfinder(
    gpuPipeline: GpuColorPipeline,
    uniforms: GpuIspUniforms,
    exposure: ExposureControls,
    stabilization: StabilizationState,
    trackedObject: TrackedObjectState,
    telemetry: RawSensorTelemetry,
    hasCameraPerm: Boolean = true,
    countdownSec: Int = 0,
    onRequestCameraPerm: () -> Unit = {},
    onTapSubjectRoi: (Float, Float) -> Unit,
    onDragAbSplit: ((Float) -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current

    SideEffect {
        gpuPipeline.uniforms = uniforms
        gpuPipeline.exposure = exposure
        gpuPipeline.trackedObject = trackedObject
    }

    val glSurfaceView = remember {
        GLSurfaceView(context).apply {
            setEGLContextClientVersion(3)
            setRenderer(gpuPipeline)
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }
    }

    DisposableEffect(glSurfaceView) {
        glSurfaceView.onResume()
        onDispose {
            glSurfaceView.onPause()
        }
    }

    Box(
        modifier = modifier
            .fillMaxSize()
            .background(OledBlack)
            .testTag("gpu_isp_viewfinder")
            .pointerInput(uniforms.abCompareSplitX > 0.02f) {
                if (uniforms.abCompareSplitX > 0.02f && onDragAbSplit != null) {
                    detectHorizontalDragGestures { change, _ ->
                        change.consume()
                        val nx = (change.position.x / size.width.coerceAtLeast(1)).coerceIn(0.08f, 0.92f)
                        onDragAbSplit(nx)
                    }
                } else {
                    detectTapGestures { offset ->
                        val nx = (offset.x / size.width.coerceAtLeast(1)).coerceIn(0.1f, 0.9f)
                        val ny = (offset.y / size.height.coerceAtLeast(1)).coerceIn(0.1f, 0.9f)
                        onTapSubjectRoi(nx, ny)
                    }
                }
            }
    ) {
        AndroidView(
            factory = { glSurfaceView },
            modifier = Modifier.fillMaxSize()
        )

        // Pro Camera Overlays: Grid, Aspect Ratio Frame Guides, 200Hz Gyro Level, MeshFlow & AF/AE Reticle
        Canvas(modifier = Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height

            // 1. Composition Grid Overlays (Rule of Thirds / Golden Ratio / Crosshair)
            val gridColor = Color.White.copy(alpha = 0.28f)
            when (uniforms.gridOverlayMode) {
                GridOverlayMode.OFF -> Unit
                GridOverlayMode.RULE_OF_THIRDS -> {
                    for (i in 1..2) {
                        val x = w * (i / 3f)
                        val y = h * (i / 3f)
                        drawLine(gridColor, Offset(x, 0f), Offset(x, h), strokeWidth = 1.dp.toPx())
                        drawLine(gridColor, Offset(0f, y), Offset(w, y), strokeWidth = 1.dp.toPx())
                    }
                }
                GridOverlayMode.GOLDEN_RATIO -> {
                    val phi = 0.382f
                    val cols = listOf(w * phi, w * (1f - phi))
                    val rows = listOf(h * phi, h * (1f - phi))
                    cols.forEach { x -> drawLine(CineAmber.copy(alpha = 0.35f), Offset(x, 0f), Offset(x, h), 1.dp.toPx()) }
                    rows.forEach { y -> drawLine(CineAmber.copy(alpha = 0.35f), Offset(0f, y), Offset(w, y), 1.dp.toPx()) }
                }
                GridOverlayMode.CENTER_CROSSHAIR -> {
                    val cx = w * 0.5f
                    val cy = h * 0.5f
                    val len = 32.dp.toPx()
                    drawLine(TacticalCyan.copy(alpha = 0.65f), Offset(cx - len, cy), Offset(cx + len, cy), 1.5.dp.toPx())
                    drawLine(TacticalCyan.copy(alpha = 0.65f), Offset(cx, cy - len), Offset(cx, cy + len), 1.5.dp.toPx())
                }
            }

            // 2. Cinema Aspect Ratio Frame Guides (2.39:1 Anamorphic, 1.85:1, 16:9, 1:1)
            val targetRatio = uniforms.aspectRatioGuide.ratioValue
            if (targetRatio > 0f) {
                val screenRatio = w / h.coerceAtLeast(1f)
                if (screenRatio < targetRatio) {
                    val boxH = w / targetRatio
                    val topBar = (h - boxH) * 0.5f
                    drawRect(OledBlack.copy(alpha = 0.58f), Offset(0f, 0f), Size(w, topBar))
                    drawRect(OledBlack.copy(alpha = 0.58f), Offset(0f, h - topBar), Size(w, topBar))
                    drawLine(CineAmber.copy(alpha = 0.7f), Offset(0f, topBar), Offset(w, topBar), 1.5.dp.toPx())
                    drawLine(CineAmber.copy(alpha = 0.7f), Offset(0f, h - topBar), Offset(w, h - topBar), 1.5.dp.toPx())
                } else {
                    val boxW = h * targetRatio
                    val sideBar = (w - boxW) * 0.5f
                    drawRect(OledBlack.copy(alpha = 0.58f), Offset(0f, 0f), Size(sideBar, h))
                    drawRect(OledBlack.copy(alpha = 0.58f), Offset(w - sideBar, 0f), Size(sideBar, h))
                    drawLine(CineAmber.copy(alpha = 0.7f), Offset(sideBar, 0f), Offset(sideBar, h), 1.5.dp.toPx())
                    drawLine(CineAmber.copy(alpha = 0.7f), Offset(w - sideBar, 0f), Offset(w - sideBar, h), 1.5.dp.toPx())
                }
            }

            // 3. Optional 16x16 MeshFlow Spatially-Variant Stabilization Vector Grid
            if (stabilization.showMeshGridOverlay) {
                val dxArr = stabilization.meshVerticesDx
                val dyArr = stabilization.meshVerticesDy
                for (r in 0 until 16) {
                    for (c in 0 until 16) {
                        val idx = r * 16 + c
                        val baseX = (c / 15f) * w
                        val baseY = (r / 15f) * h
                        val vx = if (idx < dxArr.size) dxArr[idx] * w * 1.5f else 0f
                        val vy = if (idx < dyArr.size) dyArr[idx] * h * 1.5f else 0f
                        drawCircle(
                            color = TacticalCyan.copy(alpha = 0.65f),
                            radius = 2.dp.toPx(),
                            center = Offset(baseX + vx, baseY + vy)
                        )
                        drawLine(
                            color = CineAmber.copy(alpha = 0.7f),
                            start = Offset(baseX, baseY),
                            end = Offset(baseX + vx, baseY + vy),
                            strokeWidth = 1.5.dp.toPx()
                        )
                    }
                }
            }

            // 4. Open Camera / GCam 200Hz Gyro Horizon Level Indicator
            val centerX = w * 0.5f
            val centerY = h * 0.5f
            val rollDeg = stabilization.currentSample.rollDeg
            val isLevel = abs(rollDeg) < 1.2f
            val rollRad = Math.toRadians(rollDeg.toDouble()).toFloat()
            val barHalfLen = 46.dp.toPx()
            val hx = cos(rollRad) * barHalfLen
            val hy = sin(rollRad) * barHalfLen
            val levelColor = if (isLevel) ScopeEmerald.copy(alpha = 0.85f) else Color.White.copy(alpha = 0.55f)
            drawLine(
                color = levelColor,
                start = Offset(centerX - hx, centerY - hy),
                end = Offset(centerX + hx, centerY + hy),
                strokeWidth = if (isLevel) 2.2.dp.toPx() else 1.4.dp.toPx()
            )

            // 5. Tap-to-Focus & Optical Object Tracking Reticle
            if (trackedObject.isTrackingActive) {
                val cx = trackedObject.normalizedCenterX * w
                val cy = trackedObject.normalizedCenterY * h
                val boxSize = 68.dp.toPx()
                val left = (cx - boxSize * 0.5f).coerceIn(8f, w - boxSize - 8f)
                val top = (cy - boxSize * 0.5f).coerceIn(8f, h - boxSize - 8f)

                val reticleColor = if (trackedObject.powerWindowColorMaskEnabled) CineAmber else ScopeEmerald
                val cornerLen = 14.dp.toPx()
                val strokeW = 2.dp.toPx()
                val path = Path().apply {
                    moveTo(left, top + cornerLen); lineTo(left, top); lineTo(left + cornerLen, top)
                    moveTo(left + boxSize - cornerLen, top); lineTo(left + boxSize, top); lineTo(left + boxSize, top + cornerLen)
                    moveTo(left, top + boxSize - cornerLen); lineTo(left, top + boxSize); lineTo(left + cornerLen, top + boxSize)
                    moveTo(left + boxSize - cornerLen, top + boxSize); lineTo(left + boxSize, top + boxSize); lineTo(left + boxSize, top + boxSize - cornerLen)
                }
                drawPath(path = path, color = reticleColor.copy(alpha = 0.85f), style = Stroke(width = strokeW))
                drawCircle(color = reticleColor.copy(alpha = 0.85f), radius = 2.5.dp.toPx(), center = Offset(cx, cy))
            }
        }

        // Self-Timer Countdown Overlay
        if (countdownSec > 0) {
            Box(
                modifier = Modifier
                    .align(Alignment.Center)
                    .size(96.dp)
                    .background(OledBlack.copy(alpha = 0.75f), CircleShape)
                    .border(3.dp, CineAmber, CircleShape),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "$countdownSec",
                    style = MaterialTheme.typography.headlineLarge,
                    fontWeight = FontWeight.Bold,
                    color = CineAmber
                )
            }
        }

        // Center Viewport Permission CTA if Camera Permission is not yet granted
        if (!hasCameraPerm) {
            Column(
                modifier = Modifier
                    .align(Alignment.Center)
                    .background(TacticalObsidian.copy(alpha = 0.94f), RoundedCornerShape(16.dp))
                    .border(1.5.dp, CineAmber, RoundedCornerShape(16.dp))
                    .padding(horizontal = 24.dp, vertical = 18.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Text(
                    text = "FULL-SCREEN LIVE CAMERA REQUIRES ACCESS",
                    style = MaterialTheme.typography.titleMedium,
                    color = CineAmber
                )
                Button(
                    onClick = onRequestCameraPerm,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = CineAmber,
                        contentColor = OledBlack
                    ),
                    modifier = Modifier.testTag("viewport_grant_camera_button")
                ) {
                    Text(
                        text = "ALLOW CAMERA TO STREAM LIVE VIDEO",
                        style = MaterialTheme.typography.labelLarge,
                        fontWeight = FontWeight.Bold
                    )
                }
            }
        }

        // A/B Compare Labels if Split Wipe is active
        if (uniforms.abCompareSplitX > 0.02f) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .align(Alignment.Center)
                    .padding(horizontal = 16.dp),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Box(
                    modifier = Modifier
                        .background(OledBlack.copy(alpha = 0.78f), RoundedCornerShape(6.dp))
                        .border(1.dp, CineAmber, RoundedCornerShape(6.dp))
                        .padding(horizontal = 10.dp, vertical = 4.dp)
                ) {
                    Text("A: UNGRADED LOG", style = MaterialTheme.typography.labelSmall, color = CineAmber)
                }
                Box(
                    modifier = Modifier
                        .background(OledBlack.copy(alpha = 0.78f), RoundedCornerShape(6.dp))
                        .border(1.dp, ScopeEmerald, RoundedCornerShape(6.dp))
                        .padding(horizontal = 10.dp, vertical = 4.dp)
                ) {
                    Text("B: 3-WAY LUT GRADE (DRAG WIPE)", style = MaterialTheme.typography.labelSmall, color = ScopeEmerald)
                }
            }
        }
    }
}

/**
 * Full-width Waveform & RGB Parade Scopes for the NLE Editor & Colorist Bay.
 */
@Composable
fun LiveWaveformAndParadeScopes(
    telemetry: RawSensorTelemetry,
    uniforms: GpuIspUniforms,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(52.dp)
            .background(TacticalObsidian, RoundedCornerShape(8.dp))
            .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp))
            .padding(horizontal = 10.dp, vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "LUMA WAVEFORM (IRE)",
                    style = MaterialTheme.typography.labelSmall,
                    color = CineAmber
                )
                Text(
                    text = "${uniforms.logProfile.dynamicRangeStops} STOPS DR",
                    style = MaterialTheme.typography.labelSmall,
                    color = HudTextSecondary
                )
            }
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(top = 2.dp)
            ) {
                val bins = telemetry.waveformLuma
                val colW = size.width / bins.size.coerceAtLeast(1)
                for (i in bins.indices) {
                    val v = bins[i].coerceIn(0.05f, 1.0f)
                    val barH = v * size.height
                    drawLine(
                        color = ScopeEmerald.copy(alpha = 0.85f),
                        start = Offset(i * colW, size.height),
                        end = Offset(i * colW, size.height - barH),
                        strokeWidth = (colW * 0.75f).coerceAtLeast(1.5f)
                    )
                }
            }
        }

        Box(
            modifier = Modifier
                .width(1.dp)
                .height(38.dp)
                .background(TacticalBorder)
        )

        Column(modifier = Modifier.weight(1f)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "RGB PARADE (10-BIT)",
                    style = MaterialTheme.typography.labelSmall,
                    color = TacticalCyan
                )
                Text(
                    text = if (uniforms.rec709MonitorLut) "LUT: REC.709" else "SCENE LOG",
                    style = MaterialTheme.typography.labelSmall,
                    color = if (uniforms.rec709MonitorLut) ScopeEmerald else HudTextSecondary
                )
            }
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(top = 2.dp)
            ) {
                val thirdW = size.width / 3f
                val rBins = telemetry.paradeRed
                val gBins = telemetry.paradeGreen
                val bBins = telemetry.paradeBlue
                val stepW = thirdW / rBins.size.coerceAtLeast(1)

                for (i in rBins.indices) {
                    val rh = rBins[i].coerceIn(0.05f, 1f) * size.height
                    drawLine(
                        color = Color(0xFFFF5252),
                        start = Offset(i * stepW, size.height),
                        end = Offset(i * stepW, size.height - rh),
                        strokeWidth = 2f
                    )
                    val gh = gBins[i].coerceIn(0.05f, 1f) * size.height
                    drawLine(
                        color = Color(0xFF69F0AE),
                        start = Offset(thirdW + i * stepW, size.height),
                        end = Offset(thirdW + i * stepW, size.height - gh),
                        strokeWidth = 2f
                    )
                    val bh = bBins[i].coerceIn(0.05f, 1f) * size.height
                    drawLine(
                        color = Color(0xFF40C4FF),
                        start = Offset(thirdW * 2f + i * stepW, size.height),
                        end = Offset(thirdW * 2f + i * stepW, size.height - bh),
                        strokeWidth = 2f
                    )
                }
            }
        }
    }
}
