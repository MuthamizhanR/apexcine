package com.example.ui.components

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CameraAlt
import androidx.compose.material.icons.filled.Cameraswitch
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ColorLens
import androidx.compose.material.icons.filled.Exposure
import androidx.compose.material.icons.filled.PhotoLibrary
import androidx.compose.material.icons.filled.PlayCircleFilled
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.minimumInteractiveComponentSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.example.data.ActiveControlDrawer
import com.example.data.CameraLensOption
import com.example.data.CaptureMode
import com.example.data.CineClipEntity
import com.example.data.DialParameter
import com.example.data.ExposureControls
import com.example.data.GpuIspUniforms
import com.example.data.LogTransferProfile
import com.example.data.ProCameraMode
import com.example.data.WorkspaceTab
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

/**
 * Clean, Uncluttered Full-Screen Bottom Pro Camera Console combining:
 * - Collapsible Pop-Up Pro Drawers (Manual ISO/Shutter/WB/Focus/EV, Log/LUTs, GCam HDR+/Night/Portrait & Assist)
 * - Floating Optical Lens & Zoom Switcher (`UW`, `1x`, `2x`, `TELE`, `FRONT`)
 * - Mode Carousel (`FILMIC LOG`, `MOTION RAW`, `GCAM HDR+`, `OPEN CAM`)
 * - Bottom Shutter Dock with Live Gallery Thumbnail Preview, Tactile Shutter/REC Trigger, and Quick Drawer Toggles.
 */
@Composable
fun FloatingBottomProCameraHud(
    proCameraMode: ProCameraMode,
    activeDrawer: ActiveControlDrawer,
    captureMode: CaptureMode,
    availableLenses: List<CameraLensOption>,
    selectedLens: CameraLensOption,
    exposure: ExposureControls,
    uniforms: GpuIspUniforms,
    activeDialParam: DialParameter,
    isRecording: Boolean,
    recordingElapsedSec: Float,
    showMeshOverlay: Boolean,
    latestClip: CineClipEntity?,
    latestThumbnail: Bitmap?,
    totalClipsCount: Int,
    onSelectProMode: (ProCameraMode) -> Unit,
    onToggleDrawer: (ActiveControlDrawer) -> Unit,
    onCloseDrawer: () -> Unit,
    onSelectCaptureMode: (CaptureMode) -> Unit,
    onSelectLens: (CameraLensOption) -> Unit,
    onFlipCamera: () -> Unit,
    onSetZoomRatio: (Float) -> Unit,
    onSelectDialParam: (DialParameter) -> Unit,
    onStepDialDelta: (Float) -> Unit,
    onSelectLogProfile: (LogTransferProfile) -> Unit,
    onSelectTargetFps: (Int) -> Unit,
    onToggleRec709Lut: () -> Unit,
    onToggleFalseColor: () -> Unit,
    onToggleFocusPeaking: () -> Unit,
    onToggleZebra: () -> Unit,
    onToggleMeshOverlay: () -> Unit,
    onToggleAebBracket: () -> Unit,
    onCycleAspectRatioGuide: () -> Unit,
    onHdrPlusStrengthChanged: (Float) -> Unit,
    onNightSightGainChanged: (Float) -> Unit,
    onPortraitBlurChanged: (Float) -> Unit,
    onRecordTrigger: () -> Unit,
    onCaptureSnapshotNow: () -> Unit,
    onOpenGallery: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // 1. Collapsible Pop-Up Pro Drawer (Only visible when user taps MANUAL, LOG/LUT, or ASSIST)
        when (activeDrawer) {
            ActiveControlDrawer.NONE -> Unit

            ActiveControlDrawer.MANUAL_EXPOSURE -> {
                ManualExposurePopupDrawer(
                    exposure = exposure,
                    uniforms = uniforms,
                    activeDialParam = activeDialParam,
                    onSelectDialParam = onSelectDialParam,
                    onStepDialDelta = onStepDialDelta,
                    onSelectTargetFps = onSelectTargetFps,
                    onToggleAebBracket = onToggleAebBracket,
                    onClose = onCloseDrawer
                )
            }

            ActiveControlDrawer.LOG_AND_LUT -> {
                LogAndLutPopupDrawer(
                    captureMode = captureMode,
                    uniforms = uniforms,
                    onSelectCaptureMode = onSelectCaptureMode,
                    onSelectLogProfile = onSelectLogProfile,
                    onToggleRec709Lut = onToggleRec709Lut,
                    onClose = onCloseDrawer
                )
            }

            ActiveControlDrawer.ASSIST_AND_GRIDS -> {
                AssistAndGcamPopupDrawer(
                    uniforms = uniforms,
                    showMeshOverlay = showMeshOverlay,
                    onToggleFalseColor = onToggleFalseColor,
                    onToggleFocusPeaking = onToggleFocusPeaking,
                    onToggleZebra = onToggleZebra,
                    onToggleMeshOverlay = onToggleMeshOverlay,
                    onCycleAspectRatioGuide = onCycleAspectRatioGuide,
                    onHdrPlusStrengthChanged = onHdrPlusStrengthChanged,
                    onNightSightGainChanged = onNightSightGainChanged,
                    onPortraitBlurChanged = onPortraitBlurChanged,
                    onClose = onCloseDrawer
                )
            }

            ActiveControlDrawer.STABILIZER_GYRO -> Unit
        }

        // 2. Floating Lens & Zoom Pill Row + Quick Log Curve Strip
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Hardware Lens Pills (1x MAIN, UW, TELE, FRONT)
            availableLenses.forEach { lens ->
                val isSelected = lens.cameraId == selectedLens.cameraId
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(16.dp))
                        .background(
                            if (isSelected) CineAmber else OledBlack.copy(alpha = 0.72f)
                        )
                        .border(
                            1.dp,
                            if (isSelected) CineAmber else TacticalBorder,
                            RoundedCornerShape(16.dp)
                        )
                        .clickable { onSelectLens(lens) }
                        .padding(horizontal = 12.dp, vertical = 5.dp)
                        .testTag("lens_option_${lens.cameraId}"),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = lens.label,
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = if (isSelected) OledBlack else HudTextPrimary
                    )
                }
            }

            // Quick 1x / 2x / 4x Optical/Sensor Zoom Presets
            listOf(1.0f, 2.0f, 4.0f).forEach { z ->
                val activeZoom = kotlin.math.abs(exposure.zoomRatio - z) < 0.2f
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(16.dp))
                        .background(if (activeZoom) TacticalCyan else OledBlack.copy(alpha = 0.65f))
                        .border(1.dp, if (activeZoom) TacticalCyan else TacticalBorder, RoundedCornerShape(16.dp))
                        .clickable { onSetZoomRatio(z) }
                        .padding(horizontal = 9.dp, vertical = 5.dp)
                ) {
                    Text(
                        text = "${z.toInt()}x",
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = if (activeZoom) OledBlack else HudTextSecondary
                    )
                }
            }

            // Quick Log Transfer Profile Pills (LogC3, ACEScct, S-Log3, DWG/DI)
            LogTransferProfile.entries.take(4).forEach { profile ->
                val selected = uniforms.logProfile == profile
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(16.dp))
                        .background(if (selected) ScopeEmerald else OledBlack.copy(alpha = 0.65f))
                        .border(1.dp, if (selected) ScopeEmerald else TacticalBorder, RoundedCornerShape(16.dp))
                        .clickable { onSelectLogProfile(profile) }
                        .padding(horizontal = 10.dp, vertical = 5.dp)
                        .testTag("log_profile_${profile.shortName}"),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = profile.shortName,
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = if (selected) OledBlack else HudTextPrimary
                    )
                }
            }
        }

        // 3. Pro Camera Mode Carousel (FILMIC LOG | MOTION RAW | GCAM HDR+ | OPEN CAM)
        Row(
            modifier = Modifier
                .background(OledBlack.copy(alpha = 0.68f), RoundedCornerShape(20.dp))
                .border(1.dp, TacticalBorder.copy(alpha = 0.7f), RoundedCornerShape(20.dp))
                .padding(horizontal = 6.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            ProCameraMode.entries.forEach { mode ->
                val selected = proCameraMode == mode
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(14.dp))
                        .background(if (selected) CineAmber else Color.Transparent)
                        .clickable { onSelectProMode(mode) }
                        .padding(horizontal = 12.dp, vertical = 5.dp)
                        .testTag("pro_mode_${mode.name.lowercase()}"),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = mode.title,
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = if (selected) OledBlack else HudTextPrimary
                    )
                }
            }
        }

        // 4. Bottom Shutter & Control Bar (Gallery Thumbnail | Drawer Pills | Big Shutter | Flip Camera)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(OledBlack.copy(alpha = 0.76f), RoundedCornerShape(28.dp))
                .border(1.dp, TacticalBorder.copy(alpha = 0.8f), RoundedCornerShape(28.dp))
                .padding(horizontal = 14.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Left: Live Recorded Media Thumbnail Button + Clip Counter Badge -> Opens Full-Screen Gallery
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(54.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(ElevatedSlate)
                        .border(
                            1.5.dp,
                            if (totalClipsCount > 0) CineAmber else TacticalBorder,
                            RoundedCornerShape(12.dp)
                        )
                        .clickable { onOpenGallery() }
                        .testTag("nav_tab_media_gallery"),
                    contentAlignment = Alignment.Center
                ) {
                    if (latestThumbnail != null) {
                        Image(
                            bitmap = latestThumbnail.asImageBitmap(),
                            contentDescription = "Open Recorded Media Gallery",
                            contentScale = ContentScale.Crop,
                            modifier = Modifier.fillMaxSize()
                        )
                        if (latestClip?.filePath?.endsWith(".mp4", true) == true) {
                            Icon(
                                imageVector = Icons.Filled.PlayCircleFilled,
                                contentDescription = "Video Clip",
                                tint = Color.White.copy(alpha = 0.9f),
                                modifier = Modifier.size(22.dp)
                            )
                        }
                    } else {
                        Icon(
                            imageVector = Icons.Filled.PhotoLibrary,
                            contentDescription = "Open Gallery",
                            tint = HudTextPrimary,
                            modifier = Modifier.size(24.dp)
                        )
                    }

                    if (totalClipsCount > 0) {
                        Box(
                            modifier = Modifier
                                .align(Alignment.TopEnd)
                                .padding(3.dp)
                                .background(CineAmber, CircleShape)
                                .padding(horizontal = 5.dp, vertical = 1.dp)
                        ) {
                            Text(
                                text = "$totalClipsCount",
                                style = MaterialTheme.typography.labelSmall,
                                fontWeight = FontWeight.Bold,
                                color = OledBlack
                            )
                        }
                    }
                }

                // Manual Exposure & Assist Drawer Trigger Pills
                DrawerTriggerPill(
                    icon = Icons.Filled.Exposure,
                    label = "${exposure.iso} • ${exposure.shutterFractionText}",
                    active = activeDrawer == ActiveControlDrawer.MANUAL_EXPOSURE,
                    tag = "drawer_btn_manual",
                    onClick = { onToggleDrawer(ActiveControlDrawer.MANUAL_EXPOSURE) }
                )
            }

            // Center: Tactile Pro Shutter / REC Trigger + Snapshot-while-recording button
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                val isPhotoMode = !proCameraMode.isVideoMode || captureMode == CaptureMode.RAW_PHOTO_DNG
                Box(
                    modifier = Modifier
                        .minimumInteractiveComponentSize()
                        .size(68.dp)
                        .clip(CircleShape)
                        .background(OledBlack)
                        .border(
                            3.dp,
                            if (isRecording) RecCrimson else if (isPhotoMode) CineAmber else HudTextPrimary,
                            CircleShape
                        )
                        .clickable { onRecordTrigger() }
                        .padding(6.dp)
                        .testTag("primary_record_trigger"),
                    contentAlignment = Alignment.Center
                ) {
                    if (isRecording) {
                        // Morph into Stop Square with elapsed seconds
                        Box(
                            modifier = Modifier
                                .size(30.dp)
                                .clip(RoundedCornerShape(6.dp))
                                .background(RecCrimson),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                text = "${recordingElapsedSec.toInt()}s",
                                style = MaterialTheme.typography.labelSmall,
                                fontWeight = FontWeight.Bold,
                                color = Color.White
                            )
                        }
                    } else if (isPhotoMode) {
                        // Crisp GCam / OpenCam Photo Shutter Circle
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .clip(CircleShape)
                                .background(HudTextPrimary),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Filled.CameraAlt,
                                contentDescription = "Capture Photo",
                                tint = OledBlack,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                    } else {
                        // Machined-Red Filmic Pro / MotionCam Video Record Button
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .clip(CircleShape)
                                .background(RecCrimson)
                        )
                    }
                }

                // Take Still Photo Snapshot while in Video Mode or while Recording
                if (!isPhotoMode) {
                    Box(
                        modifier = Modifier
                            .size(44.dp)
                            .clip(CircleShape)
                            .background(ElevatedSlate)
                            .border(1.dp, TacticalBorder, CircleShape)
                            .clickable { onCaptureSnapshotNow() }
                            .testTag("snapshot_while_video_button"),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Filled.CameraAlt,
                            contentDescription = "Capture Still Frame",
                            tint = HudTextPrimary,
                            modifier = Modifier.size(20.dp)
                        )
                    }
                }
            }

            // Right: Log/Assist Drawer Pills + Flip Camera Button
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                DrawerTriggerPill(
                    icon = Icons.Filled.ColorLens,
                    label = "ASSIST/HDR",
                    active = activeDrawer == ActiveControlDrawer.ASSIST_AND_GRIDS,
                    tag = "drawer_btn_assist",
                    onClick = { onToggleDrawer(ActiveControlDrawer.ASSIST_AND_GRIDS) }
                )

                Box(
                    modifier = Modifier
                        .size(46.dp)
                        .clip(CircleShape)
                        .background(ElevatedSlate)
                        .border(1.dp, TacticalBorder, CircleShape)
                        .clickable { onFlipCamera() }
                        .testTag("flip_camera_button"),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Filled.Cameraswitch,
                        contentDescription = "Flip Front/Back Camera",
                        tint = CineAmber,
                        modifier = Modifier.size(22.dp)
                    )
                }
            }
        }
    }
}

@Composable
private fun DrawerTriggerPill(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    active: Boolean,
    tag: String,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .height(40.dp)
            .clip(RoundedCornerShape(20.dp))
            .background(if (active) CineAmber else ElevatedSlate)
            .border(1.dp, if (active) CineAmber else TacticalBorder, RoundedCornerShape(20.dp))
            .clickable { onClick() }
            .padding(horizontal = 12.dp)
            .testTag(tag),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = label,
            tint = if (active) OledBlack else CineAmber,
            modifier = Modifier.size(16.dp)
        )
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.Bold,
            color = if (active) OledBlack else HudTextPrimary
        )
    }
}

@Composable
private fun ManualExposurePopupDrawer(
    exposure: ExposureControls,
    uniforms: GpuIspUniforms,
    activeDialParam: DialParameter,
    onSelectDialParam: (DialParameter) -> Unit,
    onStepDialDelta: (Float) -> Unit,
    onSelectTargetFps: (Int) -> Unit,
    onToggleAebBracket: () -> Unit,
    onClose: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(TacticalObsidian.copy(alpha = 0.92f), RoundedCornerShape(16.dp))
            .border(1.dp, CineAmber.copy(alpha = 0.7f), RoundedCornerShape(16.dp))
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "PRO MANUAL EXPOSURE & FOCUS (FILMIC PRO / OPEN CAM)",
                style = MaterialTheme.typography.labelLarge,
                color = CineAmber
            )
            Icon(
                imageVector = Icons.Filled.Close,
                contentDescription = "Close Drawer",
                tint = HudTextSecondary,
                modifier = Modifier
                    .size(20.dp)
                    .clickable { onClose() }
            )
        }

        // Parameter Selector Row
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            DialParameter.entries.forEach { param ->
                val isSelected = param == activeDialParam
                val valTxt = when (param) {
                    DialParameter.SHUTTER_ANGLE -> "${exposure.shutterAngleDeg.toInt()}° (${exposure.shutterFractionText})"
                    DialParameter.ISO_SENSITIVITY -> "${exposure.iso} ISO"
                    DialParameter.WHITE_BALANCE -> "${exposure.kelvin}K"
                    DialParameter.MANUAL_FOCUS -> if (exposure.focusDistanceDiopters < 0.05f) "AF AUTO" else "${String.format("%.1f", exposure.focusDistanceDiopters)}D"
                    DialParameter.HIGHLIGHT_RECOVERY -> "${(uniforms.highlightRecoveryStrength * 100).toInt()}%"
                    DialParameter.EV_COMPENSATION -> String.format("%+.1f EV", exposure.evCompensation)
                    DialParameter.ZOOM_RATIO -> String.format("%.1fx", exposure.zoomRatio)
                }
                Column(
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (isSelected) CineAmberDim else ElevatedSlate)
                        .border(1.dp, if (isSelected) CineAmber else TacticalBorder, RoundedCornerShape(8.dp))
                        .clickable { onSelectDialParam(param) }
                        .padding(horizontal = 10.dp, vertical = 6.dp)
                        .testTag("dial_param_${param.name.lowercase()}"),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(param.label, style = MaterialTheme.typography.labelSmall, color = if (isSelected) CineAmber else HudTextSecondary)
                    Text(valTxt, style = MaterialTheme.typography.labelLarge, color = HudTextPrimary)
                }
            }
        }

        // Fine Stepper + FPS + 3-Shot AEB Bracketing Row
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box(
                    modifier = Modifier
                        .size(34.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(ElevatedSlate)
                        .clickable { onStepDialDelta(-0.08f) }
                        .testTag("dial_step_minus"),
                    contentAlignment = Alignment.Center
                ) {
                    Text("-", style = MaterialTheme.typography.titleMedium, color = CineAmber)
                }
                Box(
                    modifier = Modifier
                        .size(34.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(ElevatedSlate)
                        .clickable { onStepDialDelta(0.08f) }
                        .testTag("dial_step_plus"),
                    contentAlignment = Alignment.Center
                ) {
                    Text("+", style = MaterialTheme.typography.titleMedium, color = CineAmber)
                }
            }

            // OpenCam 3-Shot Exposure Bracketing Toggle
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (exposure.aebBracketEnabled) ScopeEmerald else ElevatedSlate)
                    .clickable { onToggleAebBracket() }
                    .padding(horizontal = 10.dp, vertical = 6.dp)
                    .testTag("toggle_aeb_bracket")
            ) {
                Text(
                    text = if (exposure.aebBracketEnabled) "AEB 3-SHOT: ON" else "AEB 3-SHOT: OFF",
                    style = MaterialTheme.typography.labelSmall,
                    fontWeight = FontWeight.Bold,
                    color = if (exposure.aebBracketEnabled) OledBlack else HudTextPrimary
                )
            }

            // Frame Rate Selector (24 / 30 / 60 FPS)
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                listOf(24, 30, 60).forEach { fps ->
                    val active = exposure.targetFps == fps
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(if (active) TacticalCyan else ElevatedSlate)
                            .clickable { onSelectTargetFps(fps) }
                            .padding(horizontal = 9.dp, vertical = 6.dp)
                            .testTag("fps_button_$fps")
                    ) {
                        Text(
                            text = "${fps}p",
                            style = MaterialTheme.typography.labelSmall,
                            fontWeight = FontWeight.Bold,
                            color = if (active) OledBlack else HudTextPrimary
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun LogAndLutPopupDrawer(
    captureMode: CaptureMode,
    uniforms: GpuIspUniforms,
    onSelectCaptureMode: (CaptureMode) -> Unit,
    onSelectLogProfile: (LogTransferProfile) -> Unit,
    onToggleRec709Lut: () -> Unit,
    onClose: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(TacticalObsidian.copy(alpha = 0.92f), RoundedCornerShape(16.dp))
            .border(1.dp, TacticalCyan.copy(alpha = 0.7f), RoundedCornerShape(16.dp))
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "10-BIT LOG PROFILE & RECORDING FORMAT",
                style = MaterialTheme.typography.labelLarge,
                color = TacticalCyan
            )
            Icon(
                imageVector = Icons.Filled.Close,
                contentDescription = "Close Drawer",
                tint = HudTextSecondary,
                modifier = Modifier.size(20.dp).clickable { onClose() }
            )
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            CaptureMode.entries.forEach { mode ->
                val active = captureMode == mode
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (active) CineAmberDim else ElevatedSlate)
                        .border(1.dp, if (active) CineAmber else TacticalBorder, RoundedCornerShape(8.dp))
                        .clickable { onSelectCaptureMode(mode) }
                        .padding(8.dp)
                        .testTag("capture_mode_${mode.extension}"),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(mode.label, style = MaterialTheme.typography.labelSmall, color = if (active) CineAmber else HudTextPrimary)
                    Text(mode.badge, style = MaterialTheme.typography.labelSmall, color = HudTextSecondary)
                }
            }
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            LogTransferProfile.entries.forEach { profile ->
                val selected = uniforms.logProfile == profile
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (selected) CineAmber else ElevatedSlate)
                        .clickable { onSelectLogProfile(profile) }
                        .padding(vertical = 7.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = profile.shortName,
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = if (selected) OledBlack else HudTextPrimary
                    )
                }
            }
        }
    }
}

@Composable
private fun AssistAndGcamPopupDrawer(
    uniforms: GpuIspUniforms,
    showMeshOverlay: Boolean,
    onToggleFalseColor: () -> Unit,
    onToggleFocusPeaking: () -> Unit,
    onToggleZebra: () -> Unit,
    onToggleMeshOverlay: () -> Unit,
    onCycleAspectRatioGuide: () -> Unit,
    onHdrPlusStrengthChanged: (Float) -> Unit,
    onNightSightGainChanged: (Float) -> Unit,
    onPortraitBlurChanged: (Float) -> Unit,
    onClose: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(TacticalObsidian.copy(alpha = 0.92f), RoundedCornerShape(16.dp))
            .border(1.dp, ScopeEmerald.copy(alpha = 0.7f), RoundedCornerShape(16.dp))
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "GCAM HDR+ / NIGHT SIGHT & CINEMA MONITOR ASSIST",
                style = MaterialTheme.typography.labelLarge,
                color = ScopeEmerald
            )
            Icon(
                imageVector = Icons.Filled.Close,
                contentDescription = "Close Drawer",
                tint = HudTextSecondary,
                modifier = Modifier.size(20.dp).clickable { onClose() }
            )
        }

        // Cinema Assist Toggles (Peaking, Zebra, False Color, Mesh Grid, Frame Guide)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            AssistChipButton("PEAKING", uniforms.focusPeakingEnabled, ScopeEmerald, "toggle_focus_peaking", onToggleFocusPeaking)
            AssistChipButton("ZEBRA 95%", uniforms.zebraEnabled, CineAmber, "toggle_zebra", onToggleZebra)
            AssistChipButton("FALSE COLOR", uniforms.falseColorEnabled, TacticalCyan, "toggle_false_color", onToggleFalseColor)
            AssistChipButton("16x16 MESH", showMeshOverlay, TacticalCyan, "toggle_mesh_grid", onToggleMeshOverlay)
            AssistChipButton("GUIDE: ${uniforms.aspectRatioGuide.label}", uniforms.aspectRatioGuide.ratioValue > 0f, CineAmber, "cycle_aspect_guide", onCycleAspectRatioGuide)
        }

        // GCam Computational Sliders (HDR+ Tone Map, Night Sight Gain, Portrait Bokeh)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text("GCAM HDR+ TONE: ${(uniforms.hdrPlusToneMapStrength * 100).toInt()}%", style = MaterialTheme.typography.labelSmall, color = CineAmber)
                Slider(
                    value = uniforms.hdrPlusToneMapStrength,
                    onValueChange = onHdrPlusStrengthChanged,
                    valueRange = 0f..1f,
                    colors = SliderDefaults.colors(thumbColor = CineAmber, activeTrackColor = CineAmber),
                    modifier = Modifier.height(22.dp)
                )
            }
            Column(modifier = Modifier.weight(1f)) {
                Text("NIGHT SIGHT: ${(uniforms.nightSightGain * 100).toInt()}%", style = MaterialTheme.typography.labelSmall, color = TacticalCyan)
                Slider(
                    value = uniforms.nightSightGain,
                    onValueChange = onNightSightGainChanged,
                    valueRange = 0f..1f,
                    colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan),
                    modifier = Modifier.height(22.dp)
                )
            }
            Column(modifier = Modifier.weight(1f)) {
                Text("PORTRAIT BOKEH: ${(uniforms.portraitBlurStrength * 100).toInt()}%", style = MaterialTheme.typography.labelSmall, color = ScopeEmerald)
                Slider(
                    value = uniforms.portraitBlurStrength,
                    onValueChange = onPortraitBlurChanged,
                    valueRange = 0f..1f,
                    colors = SliderDefaults.colors(thumbColor = ScopeEmerald, activeTrackColor = ScopeEmerald),
                    modifier = Modifier.height(22.dp)
                )
            }
        }
    }
}

@Composable
private fun AssistChipButton(
    label: String,
    active: Boolean,
    activeColor: Color,
    tag: String,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(if (active) activeColor else ElevatedSlate)
            .clickable { onClick() }
            .padding(horizontal = 10.dp, vertical = 6.dp)
            .testTag(tag)
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.Bold,
            color = if (active) OledBlack else HudTextPrimary
        )
    }
}
