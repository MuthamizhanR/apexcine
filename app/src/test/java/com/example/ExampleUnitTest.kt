package com.example

import com.example.engine.NativeCameraOsBridge
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ExampleUnitTest {
    @Test
    fun verifyEttrHeadroomStopsAndDualConversionGain() {
        // Half-white (50% linear between black=256 and white=4095) -> ~1.0 stop headroom
        val midCode = (256 + (4095 - 256) / 2).toShort()
        val samples = ShortArray(4096) { midCode }

        val readoutLowIso = NativeCameraOsBridge.computeEttrHeadroomStops(
            bayerSamples = samples,
            blackLevel = 256,
            whiteLevel = 4095,
            currentIso = 100,
            dcgJumpIso = 400
        )
        assertEquals(1.0f, readoutLowIso.highlightHeadroomStops, 0.05f)
        assertEquals(0.0f, readoutLowIso.clippedRatio, 0.001f)
        assertFalse(readoutLowIso.isDcgHighGainActive)

        val readoutHighIso = NativeCameraOsBridge.computeEttrHeadroomStops(
            bayerSamples = samples,
            blackLevel = 256,
            whiteLevel = 4095,
            currentIso = 400,
            dcgJumpIso = 400
        )
        assertTrue(readoutHighIso.isDcgHighGainActive)
    }

    @Test
    fun verifyBayerCfaValidationAndThermalHeadroomFormatting() {
        // Standard Bayer CFA codes 0..3 (RGGB, GRBG, GBRG, BGGR) are accepted
        assertTrue(NativeCameraOsBridge.isStandardBayerCfa(0))
        assertTrue(NativeCameraOsBridge.isStandardBayerCfa(1))
        assertTrue(NativeCameraOsBridge.isStandardBayerCfa(2))
        assertTrue(NativeCameraOsBridge.isStandardBayerCfa(3))

        // RGB (4), MONO (5), NIR (6) must be rejected instead of bitmasked
        assertFalse(NativeCameraOsBridge.isStandardBayerCfa(4))
        assertFalse(NativeCameraOsBridge.isStandardBayerCfa(5))
        assertFalse(NativeCameraOsBridge.isStandardBayerCfa(6))

        // Honest thermal headroom reporting when HAL returns NaN
        assertEquals("UNAVAILABLE (HAL N/A)", NativeCameraOsBridge.formatThermalQosStatus(Float.NaN))
        assertTrue(NativeCameraOsBridge.formatThermalQosStatus(0.45f).contains("NOMINAL"))
        assertTrue(NativeCameraOsBridge.formatThermalQosStatus(0.88f).contains("HOT_30HZ_HALF_PREVIEW"))
    }
}
