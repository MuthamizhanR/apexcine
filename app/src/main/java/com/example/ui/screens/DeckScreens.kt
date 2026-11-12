package com.example.ui.screens

import android.graphics.Bitmap
import android.net.Uri
import android.widget.VideoView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.CenterFocusStrong
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Compare
import androidx.compose.material.icons.filled.ContentCut
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.MovieCreation
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PhotoCamera
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.PlayCircleFilled
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Videocam
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.data.BayerCfaPattern
import com.example.data.CineClipEntity
import com.example.data.ColorMatrixPreset
import com.example.data.ColorWheelState
import com.example.data.GalleryFilter
import com.example.data.GpuIspUniforms
import com.example.data.LutPresetEntity
import com.example.data.NleTimelineSegment
import com.example.data.OisStabilizationMode
import com.example.data.ProResExportCodec
import com.example.data.RawBitDepth
import com.example.data.RenderQueueJob
import com.example.data.ShizukuOptimizerState
import com.example.data.StabilizationState
import com.example.data.TrackedObjectState
import com.example.data.WorkspaceTab
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
import java.io.File

/**
 * Shared Top Navigation Header for Gallery, NLE Studio, and Gyro/System Screens.
 */
@Composable
fun StudioWorkspaceHeader(
    activeTab: WorkspaceTab,
    onSelectTab: (WorkspaceTab) -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(TacticalObsidian, RoundedCornerShape(14.dp))
            .border(1.dp, TacticalBorder, RoundedCornerShape(14.dp))
            .padding(horizontal = 10.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Back to Live Camera Button
        Row(
            modifier = Modifier
                .clip(RoundedCornerShape(10.dp))
                .background(CineAmber)
                .clickable { onSelectTab(WorkspaceTab.CINE_HUD) }
                .padding(horizontal = 12.dp, vertical = 6.dp)
                .testTag("back_to_camera_button"),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                contentDescription = "Back to Camera",
                tint = OledBlack,
                modifier = Modifier.size(16.dp)
            )
            Text(
                text = "LIVE CAMERA",
                style = MaterialTheme.typography.labelSmall,
                fontWeight = FontWeight.Bold,
                color = OledBlack
            )
        }

        // Workspace Tab Switcher
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf(
                WorkspaceTab.MEDIA_GALLERY,
                WorkspaceTab.NLE_TRACK,
                WorkspaceTab.GYRO_MESH
            ).forEach { tab ->
                val selected = activeTab == tab
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(10.dp))
                        .background(if (selected) CineAmberDim else ElevatedSlate)
                        .border(1.dp, if (selected) CineAmber else TacticalBorder, RoundedCornerShape(10.dp))
                        .clickable { onSelectTab(tab) }
                        .padding(horizontal = 12.dp, vertical = 6.dp)
                        .testTag("nav_tab_${tab.name.lowercase()}")
                ) {
                    Text(
                        text = tab.title,
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = if (selected) CineAmber else HudTextPrimary
                    )
                }
            }
        }
    }
}

/**
 * Built-In Multi-Track NLE Video Editor, 3-Way Colorist Wheels, Object Tracking Mask & ProRes 422 Batch Render Queue.
 */
@Composable
fun NleTrackAndGradeDeck(
    clips: List<CineClipEntity>,
    presets: List<LutPresetEntity>,
    timelineSegments: List<NleTimelineSegment>,
    renderQueue: List<RenderQueueJob>,
    playheadSec: Float,
    isPlaying: Boolean,
    liftWheel: ColorWheelState,
    gammaWheel: ColorWheelState,
    gainWheel: ColorWheelState,
    selectedClipId: Long?,
    uniforms: GpuIspUniforms,
    trackedObject: TrackedObjectState,
    selectedExportCodec: ProResExportCodec,
    bakeGyroFlow: Boolean,
    exportProgress: Float?,
    onPlayheadChanged: (Float) -> Unit,
    onTogglePlayback: () -> Unit,
    onRazorSplit: () -> Unit,
    onAddKeyframe: () -> Unit,
    onTrimSegment: (Long, Float, Float) -> Unit,
    onDragLiftWheel: (Float, Float) -> Unit,
    onDragGammaWheel: (Float, Float) -> Unit,
    onDragGainWheel: (Float, Float) -> Unit,
    onSelectClip: (CineClipEntity) -> Unit,
    onApplyPreset: (LutPresetEntity) -> Unit,
    onResetGrade: () -> Unit,
    onSelectMatrix: (ColorMatrixPreset) -> Unit,
    onSelectBayer: (BayerCfaPattern) -> Unit,
    onSelectBitDepth: (RawBitDepth) -> Unit,
    onToggleTracking: () -> Unit,
    onToggleAutoReframe: () -> Unit,
    onTogglePowerWindow: () -> Unit,
    onSelectExportCodec: (ProResExportCodec) -> Unit,
    onToggleBakeGyro: () -> Unit,
    onExportMaster: () -> Unit,
    modifier: Modifier = Modifier
) {
    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .background(TacticalObsidian, RoundedCornerShape(12.dp))
            .border(1.dp, TacticalBorder, RoundedCornerShape(12.dp))
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // 1. Multi-Track NLE Timeline Header & Transport Bar
        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, RoundedCornerShape(8.dp))
                    .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp))
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Box(
                            modifier = Modifier
                                .size(32.dp)
                                .clip(RoundedCornerShape(6.dp))
                                .background(CineAmber)
                                .clickable { onTogglePlayback() }
                                .testTag("nle_play_pause_button"),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = if (isPlaying) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                                contentDescription = "Play/Pause Timeline",
                                tint = OledBlack,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                        Text(
                            text = String.format("TC 01:00:%02d:%02d", playheadSec.toInt(), ((playheadSec % 1f) * 24).toInt()),
                            style = MaterialTheme.typography.titleMedium,
                            color = CineAmber
                        )
                    }

                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        Row(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(ElevatedSlate)
                                .border(1.dp, RecCrimson, RoundedCornerShape(6.dp))
                                .clickable { onRazorSplit() }
                                .padding(horizontal = 8.dp, vertical = 5.dp)
                                .testTag("nle_razor_split_button"),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Icon(
                                imageVector = Icons.Filled.ContentCut,
                                contentDescription = "Razor Split",
                                tint = RecCrimson,
                                modifier = Modifier.size(14.dp)
                            )
                            Spacer(modifier = Modifier.width(4.dp))
                            Text("SPLIT", style = MaterialTheme.typography.labelSmall, color = HudTextPrimary)
                        }

                        Row(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(ElevatedSlate)
                                .border(1.dp, TacticalCyan, RoundedCornerShape(6.dp))
                                .clickable { onAddKeyframe() }
                                .padding(horizontal = 8.dp, vertical = 5.dp)
                                .testTag("nle_add_keyframe_button"),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Icon(
                                imageVector = Icons.Filled.Add,
                                contentDescription = "Add Keyframe",
                                tint = TacticalCyan,
                                modifier = Modifier.size(14.dp)
                            )
                            Spacer(modifier = Modifier.width(4.dp))
                            Text("KEYFRAME", style = MaterialTheme.typography.labelSmall, color = TacticalCyan)
                        }
                    }
                }

                Slider(
                    value = playheadSec,
                    onValueChange = onPlayheadChanged,
                    valueRange = 0f..14.5f,
                    colors = SliderDefaults.colors(
                        thumbColor = RecCrimson,
                        activeTrackColor = CineAmber
                    ),
                    modifier = Modifier.height(22.dp).testTag("nle_playhead_slider")
                )

                if (timelineSegments.isEmpty()) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(ElevatedSlate)
                            .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                            .padding(10.dp)
                    ) {
                        Text(
                            text = "NO CLIPS ON TIMELINE YET — RECORD A VIDEO OR PHOTO IN CAMERA TO POPULATE TRACKS",
                            style = MaterialTheme.typography.labelSmall,
                            color = HudTextSecondary
                        )
                    }
                } else {
                    timelineSegments.forEach { seg ->
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(6.dp))
                                .background(ElevatedSlate)
                                .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                                .padding(6.dp)
                        ) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween
                            ) {
                                Text(
                                    text = "${seg.trackLabel} • ${seg.title}",
                                    style = MaterialTheme.typography.labelSmall,
                                    color = CineAmber
                                )
                                Text(
                                    text = String.format("IN %.1fs — OUT %.1fs", seg.inPointSec, seg.outPointSec),
                                    style = MaterialTheme.typography.labelSmall,
                                    color = TacticalCyan
                                )
                            }
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(
                                    text = "KEYFRAMES: ${seg.keyframes.size} PTS",
                                    style = MaterialTheme.typography.labelSmall,
                                    color = ScopeEmerald
                                )
                                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    Text(
                                        text = "[TRIM IN +0.5s]",
                                        style = MaterialTheme.typography.labelSmall,
                                        color = HudTextPrimary,
                                        modifier = Modifier.clickable {
                                            onTrimSegment(seg.segmentId, seg.inPointSec + 0.5f, seg.outPointSec)
                                        }
                                    )
                                    Text(
                                        text = "[TRIM OUT -0.5s]",
                                        style = MaterialTheme.typography.labelSmall,
                                        color = HudTextPrimary,
                                        modifier = Modifier.clickable {
                                            onTrimSegment(seg.segmentId, seg.inPointSec, seg.outPointSec - 0.5f)
                                        }
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. Interactive 3-Way Colorist Wheels (Lift / Gamma / Gain) + LUT Presets
        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, RoundedCornerShape(8.dp))
                    .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp))
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "3-WAY COLORIST WHEELS (LIFT / GAMMA / GAIN)",
                        style = MaterialTheme.typography.labelLarge,
                        color = CineAmber
                    )
                    Text(
                        text = "RESET WHEELS",
                        style = MaterialTheme.typography.labelSmall,
                        color = TacticalCyan,
                        modifier = Modifier
                            .clickable { onResetGrade() }
                            .padding(4.dp)
                            .testTag("reset_grade_button")
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceEvenly,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    ColorWheelControl(
                        title = "LIFT (SHADOWS)",
                        wheel = liftWheel,
                        onDrag = onDragLiftWheel,
                        tag = "color_wheel_lift"
                    )
                    ColorWheelControl(
                        title = "GAMMA (MIDS)",
                        wheel = gammaWheel,
                        onDrag = onDragGammaWheel,
                        tag = "color_wheel_gamma"
                    )
                    ColorWheelControl(
                        title = "GAIN (HIGHLIGHTS)",
                        wheel = gainWheel,
                        onDrag = onDragGainWheel,
                        tag = "color_wheel_gain"
                    )
                }

                LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(presets, key = { it.id }) { preset ->
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(ElevatedSlate)
                                .border(1.dp, CineAmber.copy(alpha = 0.6f), RoundedCornerShape(6.dp))
                                .clickable { onApplyPreset(preset) }
                                .padding(horizontal = 10.dp, vertical = 5.dp)
                        ) {
                            Text(
                                text = preset.name,
                                style = MaterialTheme.typography.labelSmall,
                                color = HudTextPrimary
                            )
                        }
                    }
                    items(ColorMatrixPreset.entries) { mat ->
                        val active = uniforms.colorMatrixPreset == mat
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(if (active) CineAmber else ElevatedSlate)
                                .clickable { onSelectMatrix(mat) }
                                .padding(horizontal = 8.dp, vertical = 5.dp)
                        ) {
                            Text(
                                text = mat.label,
                                style = MaterialTheme.typography.labelSmall,
                                color = if (active) OledBlack else HudTextPrimary
                            )
                        }
                    }
                }
            }
        }

        // 3. Real-Time Object Tracking Mask Editor
        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, RoundedCornerShape(8.dp))
                    .border(1.dp, TacticalCyan.copy(alpha = 0.5f), RoundedCornerShape(8.dp))
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Filled.CenterFocusStrong,
                            contentDescription = "AI Object Tracker",
                            tint = TacticalCyan,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            text = "OPTICAL OBJECT TRACKER (TAP VIEWPORT TO RE-TARGET)",
                            style = MaterialTheme.typography.labelLarge,
                            color = TacticalCyan
                        )
                    }
                    Switch(
                        checked = trackedObject.isTrackingActive,
                        onCheckedChange = { onToggleTracking() },
                        colors = SwitchDefaults.colors(checkedThumbColor = TacticalCyan),
                        modifier = Modifier.testTag("object_tracking_switch")
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (trackedObject.autoReframeLocked) TacticalCyan.copy(alpha = 0.22f) else ElevatedSlate)
                            .border(1.dp, if (trackedObject.autoReframeLocked) TacticalCyan else TacticalBorder, RoundedCornerShape(6.dp))
                            .clickable { onToggleAutoReframe() }
                            .padding(7.dp)
                            .testTag("toggle_auto_reframe")
                    ) {
                        Text(
                            text = if (trackedObject.autoReframeLocked) "AUTO-REFRAME: LOCKED" else "AUTO-REFRAME: OFF",
                            style = MaterialTheme.typography.labelSmall,
                            color = if (trackedObject.autoReframeLocked) TacticalCyan else HudTextSecondary
                        )
                    }

                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (trackedObject.powerWindowColorMaskEnabled) CineAmberDim else ElevatedSlate)
                            .border(1.dp, if (trackedObject.powerWindowColorMaskEnabled) CineAmber else TacticalBorder, RoundedCornerShape(6.dp))
                            .clickable { onTogglePowerWindow() }
                            .padding(7.dp)
                            .testTag("toggle_power_window")
                    ) {
                        Text(
                            text = if (trackedObject.powerWindowColorMaskEnabled) "POWER WINDOW MASK: ON" else "POWER WINDOW MASK: OFF",
                            style = MaterialTheme.typography.labelSmall,
                            color = if (trackedObject.powerWindowColorMaskEnabled) CineAmber else HudTextSecondary
                        )
                    }
                }
            }
        }

        // 4. Apple ProRes 422 / 10-Bit HEVC Batch Render Queue
        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, RoundedCornerShape(8.dp))
                    .border(1.dp, CineAmber, RoundedCornerShape(8.dp))
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = Icons.Filled.MovieCreation,
                        contentDescription = "ProRes 422 Export",
                        tint = CineAmber,
                        modifier = Modifier.size(16.dp)
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "PRORES 422 & 10-BIT HEVC BATCH RENDER QUEUE",
                        style = MaterialTheme.typography.labelLarge,
                        color = CineAmber
                    )
                }

                LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(ProResExportCodec.entries) { codec ->
                        val active = selectedExportCodec == codec
                        Column(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(if (active) CineAmberDim else ElevatedSlate)
                                .border(1.dp, if (active) CineAmber else TacticalBorder, RoundedCornerShape(6.dp))
                                .clickable { onSelectExportCodec(codec) }
                                .padding(horizontal = 8.dp, vertical = 5.dp)
                                .testTag("export_codec_${codec.fourCc.trim()}")
                        ) {
                            Text(
                                text = codec.label,
                                style = MaterialTheme.typography.labelSmall,
                                color = if (active) CineAmber else HudTextPrimary
                            )
                            Text(
                                text = "${codec.bitDepthLabel} • ${codec.targetMbps} Mbps",
                                style = MaterialTheme.typography.labelSmall,
                                color = HudTextSecondary
                            )
                        }
                    }
                }

                if (exportProgress != null) {
                    LinearProgressIndicator(
                        progress = { exportProgress },
                        modifier = Modifier.fillMaxWidth().height(5.dp),
                        color = ScopeEmerald,
                        trackColor = ElevatedSlate
                    )
                }

                Button(
                    onClick = onExportMaster,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = CineAmber,
                        contentColor = OledBlack
                    ),
                    modifier = Modifier
                        .fillMaxWidth()
                        .testTag("export_prores_master_button")
                ) {
                    Text(
                        text = "QUEUE & RENDER ${selectedExportCodec.label.uppercase()} (.${selectedExportCodec.containerExt.uppercase()})",
                        style = MaterialTheme.typography.labelLarge
                    )
                }

                renderQueue.take(3).forEach { job ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .background(ElevatedSlate, RoundedCornerShape(4.dp))
                            .padding(horizontal = 8.dp, vertical = 4.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "${job.clipTitle} -> ${job.codec.label}",
                            style = MaterialTheme.typography.labelSmall,
                            color = HudTextPrimary
                        )
                        Text(
                            text = if (job.isCompleted) "COMPLETED (100%)" else "${(job.progress * 100).toInt()}%",
                            style = MaterialTheme.typography.labelSmall,
                            color = if (job.isCompleted) ScopeEmerald else CineAmber
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ColorWheelControl(
    title: String,
    wheel: ColorWheelState,
    onDrag: (Float, Float) -> Unit,
    tag: String
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        Text(text = title, style = MaterialTheme.typography.labelSmall, color = HudTextSecondary)
        Box(
            modifier = Modifier
                .size(74.dp)
                .clip(CircleShape)
                .border(1.5.dp, TacticalBorder, CircleShape)
                .pointerInput(Unit) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        onDrag(
                            dragAmount.x / (size.width * 0.5f).coerceAtLeast(1f),
                            dragAmount.y / (size.height * 0.5f).coerceAtLeast(1f)
                        )
                    }
                }
                .testTag(tag),
            contentAlignment = Alignment.Center
        ) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                val r = size.minDimension * 0.5f
                val center = Offset(size.width * 0.5f, size.height * 0.5f)
                drawCircle(
                    brush = Brush.sweepGradient(
                        listOf(
                            Color(0xFFFF5252),
                            Color(0xFFFFD740),
                            Color(0xFF69F0AE),
                            Color(0xFF40C4FF),
                            Color(0xFFE040FB),
                            Color(0xFFFF5252)
                        ),
                        center = center
                    ),
                    radius = r,
                    alpha = 0.32f
                )
                drawLine(
                    color = TacticalBorder,
                    start = Offset(0f, center.y),
                    end = Offset(size.width, center.y),
                    strokeWidth = 1f
                )
                drawLine(
                    color = TacticalBorder,
                    start = Offset(center.x, 0f),
                    end = Offset(center.x, size.height),
                    strokeWidth = 1f
                )
                val px = center.x + wheel.puckX * (r * 0.75f)
                val py = center.y + wheel.puckY * (r * 0.75f)
                drawCircle(
                    color = CineAmber,
                    radius = 5.dp.toPx(),
                    center = Offset(px, py)
                )
                drawCircle(
                    color = OledBlack,
                    radius = 5.dp.toPx(),
                    center = Offset(px, py),
                    style = Stroke(width = 1.5.dp.toPx())
                )
            }
        }
    }
}

/**
 * Built-In Media Bin Gallery with Real Visual Thumbnails, Integrated `.MP4` Video Player & `.JPG` Photo Viewer,
 * Multi-Channel RGB Histogram & Side-by-Side A/B Wipe Comparator.
 */
@Composable
fun MediaBinGalleryDeck(
    clips: List<CineClipEntity>,
    selectedClipId: Long?,
    activeFilter: GalleryFilter,
    abCompareEnabled: Boolean,
    telemetry: RawSensorTelemetry,
    getThumbnailForClip: (String) -> Bitmap?,
    onSelectFilter: (GalleryFilter) -> Unit,
    onSelectClip: (CineClipEntity) -> Unit,
    onToggleAbCompare: (Boolean) -> Unit,
    onOpenInNle: (CineClipEntity) -> Unit,
    onDeleteClip: (Long) -> Unit,
    onBackToCamera: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val filteredClips = clips.filter { clip ->
        when (activeFilter) {
            GalleryFilter.ALL -> true
            GalleryFilter.MCRAW_V3 -> clip.filePath.endsWith(".mp4", true) || clip.filePath.endsWith(".mcraw", true)
            GalleryFilter.RAW_DNG -> clip.filePath.endsWith(".jpg", true) || clip.filePath.endsWith(".dng", true)
            GalleryFilter.PRORES_MASTER -> clip.lastExportCodec.isNotEmpty()
        }
    }
    val inspectedClip = filteredClips.find { it.id == selectedClipId } ?: filteredClips.firstOrNull()
    var isVideoPlayingNative by remember(inspectedClip?.id) { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(TacticalObsidian, RoundedCornerShape(12.dp))
            .border(1.dp, TacticalBorder, RoundedCornerShape(12.dp))
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // Top Filter Bar + Side-by-Side A/B Split Wipe Toggle
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            LazyRow(
                modifier = Modifier.weight(1f),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                items(GalleryFilter.entries) { filter ->
                    val selected = filter == activeFilter
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(if (selected) CineAmber else ElevatedSlate)
                            .clickable { onSelectFilter(filter) }
                            .padding(horizontal = 10.dp, vertical = 6.dp)
                            .testTag("gallery_filter_${filter.name.lowercase()}")
                    ) {
                        Text(
                            text = filter.label,
                            style = MaterialTheme.typography.labelSmall,
                            fontWeight = FontWeight.Bold,
                            color = if (selected) OledBlack else HudTextPrimary
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.width(8.dp))

            Row(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (abCompareEnabled) ScopeEmerald.copy(alpha = 0.22f) else ElevatedSlate)
                    .border(1.dp, if (abCompareEnabled) ScopeEmerald else TacticalBorder, RoundedCornerShape(8.dp))
                    .clickable { onToggleAbCompare(!abCompareEnabled) }
                    .padding(horizontal = 10.dp, vertical = 6.dp)
                    .testTag("gallery_ab_compare_toggle"),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Filled.Compare,
                    contentDescription = "A/B Split Compare",
                    tint = if (abCompareEnabled) ScopeEmerald else TacticalCyan,
                    modifier = Modifier.size(15.dp)
                )
                Spacer(modifier = Modifier.width(4.dp))
                Text(
                    text = if (abCompareEnabled) "A/B WIPE: ACTIVE" else "A/B SPLIT COMPARE",
                    style = MaterialTheme.typography.labelSmall,
                    color = if (abCompareEnabled) ScopeEmerald else HudTextPrimary
                )
            }
        }

        if (filteredClips.isEmpty()) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(OledBlack, RoundedCornerShape(10.dp))
                    .border(1.dp, TacticalBorder, RoundedCornerShape(10.dp))
                    .padding(20.dp),
                contentAlignment = Alignment.Center
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = "NO RECORDED VIDEOS OR PHOTOS YET",
                        style = MaterialTheme.typography.titleMedium,
                        color = CineAmber
                    )
                    Text(
                        text = "Record a video or capture a photo in the full-screen camera and it will appear here immediately with its visual thumbnail and video player.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = HudTextSecondary
                    )
                    Button(
                        onClick = onBackToCamera,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = CineAmber,
                            contentColor = OledBlack
                        )
                    ) {
                        Text("RETURN TO LIVE CAMERA & RECORD", style = MaterialTheme.typography.labelLarge)
                    }
                }
            }
        } else {
            // Split Pane: Left Visual Thumbnail List + Right Built-In Video/Photo Player & Inspector
            Row(
                modifier = Modifier.fillMaxSize(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                LazyColumn(
                    modifier = Modifier.weight(0.46f).fillMaxHeight(),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(filteredClips, key = { it.id }) { clip ->
                        val selected = clip.id == inspectedClip?.id
                        val thumbBmp = remember(clip.filePath) { getThumbnailForClip(clip.filePath) }
                        val isVideo = clip.filePath.endsWith(".mp4", ignoreCase = true)

                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(10.dp))
                                .background(if (selected) CineAmberDim else ElevatedSlate)
                                .border(
                                    1.5.dp,
                                    if (selected) CineAmber else TacticalBorder,
                                    RoundedCornerShape(10.dp)
                                )
                                .clickable { onSelectClip(clip) }
                                .padding(8.dp)
                                .testTag("gallery_clip_${clip.id}"),
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            // Visual Thumbnail Box
                            Box(
                                modifier = Modifier
                                    .size(width = 84.dp, height = 54.dp)
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(OledBlack)
                                    .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp)),
                                contentAlignment = Alignment.Center
                            ) {
                                if (thumbBmp != null) {
                                    Image(
                                        bitmap = thumbBmp.asImageBitmap(),
                                        contentDescription = clip.title,
                                        contentScale = ContentScale.Crop,
                                        modifier = Modifier.fillMaxSize()
                                    )
                                }
                                Icon(
                                    imageVector = if (isVideo) Icons.Filled.PlayCircleFilled else Icons.Filled.PhotoCamera,
                                    contentDescription = null,
                                    tint = Color.White.copy(alpha = 0.88f),
                                    modifier = Modifier.size(22.dp)
                                )
                            }

                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = clip.title,
                                    style = MaterialTheme.typography.labelLarge,
                                    color = HudTextPrimary,
                                    maxLines = 1
                                )
                                Text(
                                    text = if (isVideo) "VIDEO • ${String.format("%.1fs", clip.durationSec)} • ${clip.fps}p"
                                    else "PHOTO • ${clip.captureMode}",
                                    style = MaterialTheme.typography.labelSmall,
                                    color = CineAmber
                                )
                                Text(
                                    text = "${clip.bayerFormatName} • ${clip.logProfileName}",
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = HudTextSecondary,
                                    maxLines = 1
                                )
                            }
                        }
                    }
                }

                // Right Built-In Video / Photo Player & Metadata Inspector
                if (inspectedClip != null) {
                    val isVideo = inspectedClip.filePath.endsWith(".mp4", ignoreCase = true)
                    val inspectedThumb = remember(inspectedClip.filePath) {
                        getThumbnailForClip(inspectedClip.filePath)
                    }
                    val fileSizeKb = remember(inspectedClip.filePath) {
                        (File(inspectedClip.filePath).length() / 1024L).coerceAtLeast(12L)
                    }

                    Column(
                        modifier = Modifier
                            .weight(0.54f)
                            .fillMaxHeight()
                            .background(OledBlack, RoundedCornerShape(10.dp))
                            .border(1.dp, TacticalBorder, RoundedCornerShape(10.dp))
                            .padding(10.dp),
                        verticalArrangement = Arrangement.SpaceBetween
                    ) {
                        // Built-in Visual Media Preview / Video Player Box
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f)
                                .clip(RoundedCornerShape(8.dp))
                                .background(TacticalObsidian)
                                .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp)),
                            contentAlignment = Alignment.Center
                        ) {
                            if (isVideo && isVideoPlayingNative && File(inspectedClip.filePath).exists()) {
                                AndroidView(
                                    factory = { ctx ->
                                        VideoView(ctx).apply {
                                            setVideoURI(Uri.fromFile(File(inspectedClip.filePath)))
                                            setOnPreparedListener { mp ->
                                                mp.isLooping = true
                                                start()
                                            }
                                            setOnErrorListener { _, _, _ ->
                                                isVideoPlayingNative = false
                                                true
                                            }
                                        }
                                    },
                                    modifier = Modifier.fillMaxSize()
                                )
                            } else if (inspectedThumb != null) {
                                Image(
                                    bitmap = inspectedThumb.asImageBitmap(),
                                    contentDescription = inspectedClip.title,
                                    contentScale = ContentScale.Fit,
                                    modifier = Modifier.fillMaxSize()
                                )
                            }

                            if (isVideo) {
                                Box(
                                    modifier = Modifier
                                        .align(Alignment.BottomEnd)
                                        .padding(8.dp)
                                        .clip(RoundedCornerShape(16.dp))
                                        .background(CineAmber)
                                        .clickable { isVideoPlayingNative = !isVideoPlayingNative }
                                        .padding(horizontal = 12.dp, vertical = 6.dp)
                                        .testTag("gallery_play_video_button")
                                ) {
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                                    ) {
                                        Icon(
                                            imageVector = if (isVideoPlayingNative) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                                            contentDescription = "Play/Pause Video",
                                            tint = OledBlack,
                                            modifier = Modifier.size(16.dp)
                                        )
                                        Text(
                                            text = if (isVideoPlayingNative) "PAUSE MP4" else "PLAY MP4",
                                            style = MaterialTheme.typography.labelSmall,
                                            fontWeight = FontWeight.Bold,
                                            color = OledBlack
                                        )
                                    }
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(6.dp))

                        // Metadata & RGB Histogram Strip
                        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                            Text(
                                text = "${inspectedClip.title} (${fileSizeKb} KB)",
                                style = MaterialTheme.typography.labelLarge,
                                color = HudTextPrimary,
                                maxLines = 1
                            )
                            Text(
                                text = "SAVED: ${inspectedClip.filePath}",
                                style = MaterialTheme.typography.labelSmall,
                                color = HudTextSecondary,
                                maxLines = 1
                            )
                        }

                        Spacer(modifier = Modifier.height(6.dp))

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Button(
                                onClick = { onOpenInNle(inspectedClip) },
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = CineAmber,
                                    contentColor = OledBlack
                                ),
                                modifier = Modifier.weight(1f).testTag("open_in_nle_button")
                            ) {
                                Text("EDIT & GRADE IN NLE", style = MaterialTheme.typography.labelSmall, fontWeight = FontWeight.Bold)
                            }
                            Box(
                                modifier = Modifier
                                    .size(38.dp)
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(ElevatedSlate)
                                    .border(1.dp, RecCrimson, RoundedCornerShape(8.dp))
                                    .clickable { onDeleteClip(inspectedClip.id) }
                                    .testTag("delete_clip_button"),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Filled.Delete,
                                    contentDescription = "Delete Clip",
                                    tint = RecCrimson,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Combined 200Hz GyroFlow + 16x16 MeshFlow Stabilizer & Hardware Performance Governor Deck.
 */
@Composable
fun GyroAndShizukuSystemDeck(
    stabilization: StabilizationState,
    optimizerState: ShizukuOptimizerState,
    onSelectOisMode: (OisStabilizationMode) -> Unit,
    onRollingShutterChanged: (Float) -> Unit,
    onSmoothingChanged: (Float) -> Unit,
    onHorizonLockChanged: (Boolean) -> Unit,
    onRecenterGyro: () -> Unit,
    onEngageCineBoost: () -> Unit,
    onToggleOptions: (Boolean, Boolean, Boolean, Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .background(TacticalObsidian, RoundedCornerShape(12.dp))
            .border(1.dp, TacticalBorder, RoundedCornerShape(12.dp))
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, RoundedCornerShape(10.dp))
                    .border(
                        1.dp,
                        if (optimizerState.cineBoostEngaged) ScopeEmerald else CineAmber,
                        RoundedCornerShape(10.dp)
                    )
                    .padding(10.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Filled.Bolt,
                            contentDescription = "Performance Boost",
                            tint = if (optimizerState.cineBoostEngaged) ScopeEmerald else CineAmber
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Column {
                            Text(
                                text = "HARDWARE CAMERA & GPU COMPUTE GOVERNOR",
                                style = MaterialTheme.typography.labelLarge,
                                color = HudTextPrimary
                            )
                            Text(
                                text = "RAM RECLAIMED: +${optimizerState.freedRamMb} MB • BUFFER POOL: ${optimizerState.writerQueueUsedSlabs}/${optimizerState.writerQueueTotalSlabs} SLABS",
                                style = MaterialTheme.typography.labelSmall,
                                color = ScopeEmerald
                            )
                        }
                    }

                    Button(
                        onClick = onEngageCineBoost,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (optimizerState.cineBoostEngaged) ScopeEmerald else CineAmber,
                            contentColor = OledBlack
                        ),
                        modifier = Modifier.testTag("engage_shizuku_boost_button")
                    ) {
                        Icon(Icons.Filled.Speed, contentDescription = null, modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text(
                            text = if (optimizerState.cineBoostEngaged) "RE-BOOST" else "CINE BOOST",
                            style = MaterialTheme.typography.labelLarge
                        )
                    }
                }
                Text(
                    text = optimizerState.lastCommandLog,
                    style = MaterialTheme.typography.bodyMedium,
                    color = HudTextSecondary
                )
            }
        }

        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, RoundedCornerShape(10.dp))
                    .border(1.dp, TacticalBorder, RoundedCornerShape(10.dp))
                    .padding(10.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "200Hz GYROFLOW + 16x16 MESHFLOW ENGINE",
                        style = MaterialTheme.typography.titleMedium,
                        color = TacticalCyan
                    )
                    Button(
                        onClick = onRecenterGyro,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = ElevatedSlate,
                            contentColor = CineAmber
                        ),
                        modifier = Modifier.testTag("recenter_gyro_button")
                    ) {
                        Icon(Icons.Filled.Refresh, contentDescription = "Recenter Gyro", modifier = Modifier.size(14.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("ZERO IMU", style = MaterialTheme.typography.labelSmall)
                    }
                }

                OisStabilizationMode.entries.forEach { mode ->
                    val selected = stabilization.oisMode == mode
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (selected) CineAmberDim else ElevatedSlate)
                            .border(1.dp, if (selected) CineAmber else TacticalBorder, RoundedCornerShape(6.dp))
                            .clickable { onSelectOisMode(mode) }
                            .padding(8.dp)
                            .testTag("ois_mode_${mode.name.lowercase()}"),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = mode.label,
                                style = MaterialTheme.typography.labelLarge,
                                color = if (selected) CineAmber else HudTextPrimary
                            )
                            Text(
                                text = mode.detail,
                                style = MaterialTheme.typography.bodyMedium,
                                color = HudTextSecondary
                            )
                        }
                        if (selected) {
                            Icon(Icons.Filled.CheckCircle, contentDescription = null, tint = CineAmber)
                        }
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("CMOS ROLLING SHUTTER READOUT", style = MaterialTheme.typography.labelSmall, color = CineAmber)
                    Text("${String.format("%.1f", stabilization.rollingShutterReadoutMs)} ms", style = MaterialTheme.typography.labelSmall, color = HudTextPrimary)
                }
                Slider(
                    value = stabilization.rollingShutterReadoutMs,
                    onValueChange = onRollingShutterChanged,
                    valueRange = 2.0f..22.0f,
                    colors = SliderDefaults.colors(thumbColor = CineAmber, activeTrackColor = CineAmber),
                    modifier = Modifier.height(24.dp).testTag("rolling_shutter_slider")
                )

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("QUATERNION SLERP DAMPING", style = MaterialTheme.typography.labelSmall, color = TacticalCyan)
                    Text("${(stabilization.smoothingStrength * 100).toInt()}%", style = MaterialTheme.typography.labelSmall, color = HudTextPrimary)
                }
                Slider(
                    value = stabilization.smoothingStrength,
                    onValueChange = onSmoothingChanged,
                    valueRange = 0.05f..0.98f,
                    colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan),
                    modifier = Modifier.height(24.dp).testTag("gyro_smoothing_slider")
                )

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "360° HORIZON ROLL LOCK",
                        style = MaterialTheme.typography.labelSmall,
                        color = HudTextPrimary
                    )
                    Switch(
                        checked = stabilization.horizonLockEnabled,
                        onCheckedChange = onHorizonLockChanged,
                        colors = SwitchDefaults.colors(checkedThumbColor = ScopeEmerald),
                        modifier = Modifier.testTag("horizon_lock_switch")
                    )
                }
            }
        }
    }
}
