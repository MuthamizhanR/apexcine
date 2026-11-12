package com.example

import android.content.Context
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ApplicationProvider
import com.example.data.BayerCfaPattern
import com.example.data.CaptureMode
import com.example.data.LogTransferProfile
import com.example.engine.GpuColorPipeline
import com.example.engine.GyroMeshStabilizer
import com.example.engine.NativeCameraOsBridge
import com.example.engine.RawIspBypassEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34], qualifiers = "w411dp-h891dp-xxhdpi")
class ExampleRobolectricTest {

    @get:Rule
    val composeTestRule = createAndroidComposeRule<MainActivity>()

    @Test
    fun verifyVulkanComputeContextDebayeringAndRealVideoRecording() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val rawEngine = RawIspBypassEngine(context)
        val stabilizer = GyroMeshStabilizer(context)
        val pipeline = GpuColorPipeline(context, rawEngine, stabilizer)

        // 1. Verify Vulkan Compute Shader Context Initialization
        val vkCtx = pipeline.initializeVulkanComputeContext()
        assertTrue(vkCtx.isInitialized)
        assertEquals(0x07230203, vkCtx.spirvMagicWord)

        // 2. Construct a 16-bit RGGB Bayer buffer (640x360)
        val width = pipeline.pipelineWidth
        val height = pipeline.pipelineHeight
        val rawBayer = ByteBuffer.allocateDirect(width * height * 2).order(ByteOrder.LITTLE_ENDIAN)
        val shortView = rawBayer.asShortBuffer()
        for (y in 0 until height) {
            for (x in 0 until width) {
                val isEvenY = (y and 1) == 0
                val isEvenX = (x and 1) == 0
                val code14Bit = when {
                    isEvenY && isEvenX -> 6400
                    !isEvenY && !isEvenX -> 5200
                    else -> 6000
                }
                shortView.put(y * width + x, code14Bit.toShort())
            }
        }

        // 3. Feed raw Bayer buffer into GpuColorPipeline
        val logFrame = pipeline.acceptRawBayerBufferFromCamera2Interop(
            rawSensorPlane = rawBayer,
            srcWidth = width,
            srcHeight = height,
            rowStride = width * 2,
            cfaPattern = BayerCfaPattern.RGGB,
            logProfile = LogTransferProfile.ARRI_LOG_C3
        )
        val (r10, g10, b10) = logFrame.read10BitRgbAt(width / 2, height / 2)
        assertTrue(r10 in 200..980)
        assertTrue(g10 in 200..980)
        assertTrue(b10 in 200..980)

        // 4. Verify real MP4 video recording creates a non-empty .mp4 file AND a visual thumbnail .jpg
        rawEngine.startRecording(CaptureMode.DIRECT_LOG_10BIT)
        val savedClip = rawEngine.stopRecording(CaptureMode.DIRECT_LOG_10BIT, 8.4f)
        assertNotNull(savedClip)
        assertTrue(File(savedClip!!.filePath).exists())
        assertTrue(File(savedClip.filePath).length() > 0L)
        assertNotNull(rawEngine.getClipThumbnailBitmap(savedClip.filePath))

        // 5. Verify data-driven Device Quirk DB matches OnePlus Nord 5 + target device matrix
        val nord5 = NativeCameraOsBridge.matchDeviceProfileFromAssets(
            context, "OnePlus", "OnePlus Nord 5 CPH2709"
        )
        assertEquals("oneplus_nord5_lyt600", nord5.profileId)
        assertEquals(400, nord5.dcgJumpIso)
        assertFalse(nord5.isHardwareVerified)
        assertFalse(nord5.isColorimetricallyCalibrated)
        assertTrue(nord5.measuredMeanDeltaE2000D50 < 0f)

        val pixel9 = NativeCameraOsBridge.matchDeviceProfileFromAssets(
            context, "Google", "Pixel 9 Pro"
        )
        assertEquals("google_pixel9pro_gnk", pixel9.profileId)

        // 6. Verify manufacturer-only match inherits ONLY quirk flags, not Nord 5 sensor specs
        val oneplus7t = NativeCameraOsBridge.matchDeviceProfileFromAssets(
            context, "OnePlus", "OnePlus 7T HD1905"
        )
        assertEquals("oneplus_generic_quirks_fallback", oneplus7t.profileId)
        assertFalse(oneplus7t.primarySensorName.contains("LYT-600"))

        // 7. Verify unknown OEM falls back to generic_android_camera2, NOT Nord 5
        val unknownDevice = NativeCameraOsBridge.matchDeviceProfileFromAssets(
            context, "Fairphone", "Fairphone 5"
        )
        assertEquals("generic_android_camera2", unknownDevice.profileId)

        pipeline.release()
        rawEngine.release()
    }

    @Test
    fun verifyFullScreenProCameraModesDrawersAndGalleryRecording() {
        composeTestRule.onNodeWithTag("cpu_gpu_telemetry_bar").assertIsDisplayed()
        composeTestRule.onNodeWithTag("hud_ettr_stops_badge").assertIsDisplayed()
        composeTestRule.onNodeWithTag("gpu_isp_viewfinder").assertIsDisplayed()
        composeTestRule.onNodeWithTag("primary_record_trigger").assertIsDisplayed()

        // Switch Pro Camera Modes (Filmic Log -> GCam HDR+ -> Open Cam -> Motion Raw)
        composeTestRule.onNodeWithTag("pro_mode_gcam_computational").performClick()
        composeTestRule.onNodeWithTag("pro_mode_opencam_manual").performClick()
        composeTestRule.onNodeWithTag("pro_mode_filmic_pro_video").performClick()

        // Open Manual Exposure Drawer & adjust parameter
        composeTestRule.onNodeWithTag("drawer_btn_manual").performClick()
        composeTestRule.onNodeWithTag("dial_step_plus").performClick()
        composeTestRule.onNodeWithTag("drawer_btn_manual").performClick()

        // Open Gallery via Bottom-Left Thumbnail Button
        composeTestRule.onNodeWithTag("nav_tab_media_gallery").performClick()
        composeTestRule.onNodeWithTag("gallery_ab_compare_toggle").performClick()
        composeTestRule.onNodeWithText("A/B WIPE: ACTIVE").assertIsDisplayed()

        // Navigate to NLE Studio & Gyro/System tabs from StudioWorkspaceHeader
        composeTestRule.onNodeWithTag("nav_tab_nle_track").performClick()
        composeTestRule.onNodeWithTag("color_wheel_lift").assertIsDisplayed()

        composeTestRule.onNodeWithTag("nav_tab_gyro_mesh").performClick()
        composeTestRule.onNodeWithTag("camera_os_diagnostics_panel").assertIsDisplayed()
        composeTestRule.onNodeWithTag("diagnostics_profile_verification_badge").assertIsDisplayed()
        composeTestRule.onNodeWithTag("engage_shizuku_boost_button").performClick()
        composeTestRule.onNodeWithText("RE-BOOST").assertIsDisplayed()

        // Return to Full-Screen Live Camera
        composeTestRule.onNodeWithTag("back_to_camera_button").performClick()
        composeTestRule.onNodeWithTag("primary_record_trigger").assertIsDisplayed()
    }
}
