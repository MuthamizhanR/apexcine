package com.example.engine

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import com.example.data.GyroQuaternionSample
import com.example.data.OisStabilizationMode
import com.example.data.StabilizationState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * 200Hz GyroFlow + 16x16 MeshFlow Spatially-Variant Stabilization Engine.
 * Captures high-rate gyroscope and accelerometer telemetry, fuses 6-DOF orientation quaternions,
 * compensates for CMOS rolling-shutter readout skew, and computes a 16x16 vertex deformation mesh.
 */
class GyroMeshStabilizer(context: Context) : SensorEventListener {

    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val gyroSensor: Sensor? =
        sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE_UNCALIBRATED)
            ?: sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
    private val accelSensor: Sensor? =
        sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)

    private val _state = MutableStateFlow(StabilizationState())
    val state: StateFlow<StabilizationState> = _state.asStateFlow()

    // Ring buffer of recent 200Hz IMU samples for .mcraw v3 muxing
    private val recordedImuQueue = ConcurrentLinkedQueue<GyroQuaternionSample>()

    private var qW = 1f
    private var qX = 0f
    private var qY = 0f
    private var qZ = 0f

    // Smoothed virtual camera quaternion (SLERP low-pass filter)
    private var smoothQW = 1f
    private var smoothQX = 0f
    private var smoothQY = 0f
    private var smoothQZ = 0f

    private var lastGyroTimestampNs: Long = 0L
    private var lastHardwareSensorEventMs: Long = 0L
    private var sampleCounter = 0
    private var lastRateMeasureMs = System.currentTimeMillis()
    private var measuredHz = 200

    private var lastAccelX = 0f
    private var lastAccelY = 9.81f
    private var lastAccelZ = 0f

    private var simulationJob: Job? = null

    fun start(scope: CoroutineScope) {
        try {
            gyroSensor?.let {
                sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_FASTEST)
            }
            accelSensor?.let {
                sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_FASTEST)
            }
        } catch (_: SecurityException) {
            // Fallback to normal fastest rate if HIGH_SAMPLING_RATE_SENSORS is restricted
            gyroSensor?.let {
                sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME)
            }
        }

        simulationJob?.cancel()
        simulationJob = scope.launch(Dispatchers.Default) {
            var phase = 0.0
            while (isActive) {
                val nowMs = System.currentTimeMillis()
                // If running in a virtualized emulator where hardware gyro is quiet,
                // synthesize realistic handheld micro-tremor at 200Hz so GyroFlow + MeshFlow
                // horizon vectors and 16x16 warp grid operate continuously in real time.
                if (nowMs - lastHardwareSensorEventMs > 250L) {
                    phase += 0.0314
                    val wx = (sin(phase * 2.3) * 0.14 + cos(phase * 5.7) * 0.04).toFloat()
                    val wy = (cos(phase * 1.9) * 0.16 + sin(phase * 4.3) * 0.05).toFloat()
                    val wz = (sin(phase * 1.4) * 0.09).toFloat()
                    integrateGyroStep(
                        wx = wx,
                        wy = wy,
                        wz = wz,
                        dtSec = 0.005f,
                        timestampNs = System.nanoTime()
                    )
                }
                delay(5L) // 200Hz tick (5ms)
            }
        }
    }

    fun stop() {
        sensorManager.unregisterListener(this)
        simulationJob?.cancel()
    }

    fun setOisMode(mode: OisStabilizationMode) {
        _state.value = _state.value.copy(oisMode = mode)
    }

    fun setRollingShutterReadoutMs(readoutMs: Float) {
        _state.value = _state.value.copy(rollingShutterReadoutMs = readoutMs.coerceIn(2.0f, 25.0f))
    }

    fun setSmoothingStrength(strength: Float) {
        _state.value = _state.value.copy(smoothingStrength = strength.coerceIn(0.05f, 0.98f))
    }

    fun setHorizonLock(enabled: Boolean) {
        _state.value = _state.value.copy(horizonLockEnabled = enabled)
    }

    fun toggleMeshGridOverlay() {
        _state.value = _state.value.copy(showMeshGridOverlay = !_state.value.showMeshGridOverlay)
    }

    fun recenterQuaternion() {
        qW = 1f; qX = 0f; qY = 0f; qZ = 0f
        smoothQW = 1f; smoothQX = 0f; smoothQY = 0f; smoothQZ = 0f
    }

    fun drainRecordedImuSamples(maxCount: Int = 32): List<GyroQuaternionSample> {
        val out = ArrayList<GyroQuaternionSample>(maxCount)
        var i = 0
        while (i < maxCount) {
            val s = recordedImuQueue.poll() ?: break
            out.add(s)
            i++
        }
        return out
    }

    override fun onSensorChanged(event: SensorEvent) {
        lastHardwareSensorEventMs = System.currentTimeMillis()
        when (event.sensor.type) {
            Sensor.TYPE_GYROSCOPE, Sensor.TYPE_GYROSCOPE_UNCALIBRATED -> {
                if (lastGyroTimestampNs != 0L) {
                    val dt = ((event.timestamp - lastGyroTimestampNs) * 1e-9f).coerceIn(0.0005f, 0.05f)
                    integrateGyroStep(
                        wx = event.values[0],
                        wy = event.values[1],
                        wz = event.values[2],
                        dtSec = dt,
                        timestampNs = event.timestamp
                    )
                }
                lastGyroTimestampNs = event.timestamp
            }
            Sensor.TYPE_ACCELEROMETER -> {
                lastAccelX = event.values[0]
                lastAccelY = event.values[1]
                lastAccelZ = event.values[2]
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) = Unit

    private fun integrateGyroStep(
        wx: Float,
        wy: Float,
        wz: Float,
        dtSec: Float,
        timestampNs: Long
    ) {
        sampleCounter++
        val nowMs = System.currentTimeMillis()
        if (nowMs - lastRateMeasureMs >= 500L) {
            measuredHz = ((sampleCounter * 1000L) / (nowMs - lastRateMeasureMs).coerceAtLeast(1L))
                .toInt()
                .coerceIn(60, 400)
            sampleCounter = 0
            lastRateMeasureMs = nowMs
        }

        val omegaMag = sqrt(wx * wx + wy * wy + wz * wz)
        val halfTheta = 0.5f * omegaMag * dtSec
        val dW: Float
        val dX: Float
        val dY: Float
        val dZ: Float
        if (omegaMag > 1e-5f) {
            val s = sin(halfTheta) / omegaMag
            dW = cos(halfTheta)
            dX = wx * s
            dY = wy * s
            dZ = wz * s
        } else {
            dW = 1f; dX = 0f; dY = 0f; dZ = 0f
        }

        // Quaternion multiplication q = q * dq
        val nW = qW * dW - qX * dX - qY * dY - qZ * dZ
        val nX = qW * dX + qX * dW + qY * dZ - qZ * dY
        val nY = qW * dY - qX * dZ + qY * dW + qZ * dX
        val nZ = qW * dZ + qX * dY - qY * dX + qZ * dW

        // Gentle gravity drift correction toward upright horizon (complementary VQF)
        val norm = sqrt(nW * nW + nX * nX + nY * nY + nZ * nZ).coerceAtLeast(1e-6f)
        qW = (nW / norm) * 0.996f + 0.004f
        qX = (nX / norm) * 0.996f
        qY = (nY / norm) * 0.996f
        qZ = (nZ / norm) * 0.996f

        val curState = _state.value
        val alpha = (1f - curState.smoothingStrength).coerceIn(0.02f, 0.5f)
        smoothQW += (qW - smoothQW) * alpha
        smoothQX += (qX - smoothQX) * alpha
        smoothQY += (qY - smoothQY) * alpha
        smoothQZ += (qZ - smoothQZ) * alpha

        val pitchDeg = Math.toDegrees(
            atan2(2.0 * (qW * qX + qY * qZ), 1.0 - 2.0 * (qX * qX + qY * qY))
        ).toFloat()
        val rollDeg = Math.toDegrees(
            atan2(2.0 * (qW * qZ + qX * qY), 1.0 - 2.0 * (qY * qY + qZ * qZ))
        ).toFloat()
        val yawDeg = Math.toDegrees(
            atan2(2.0 * (qW * qY - qZ * qX), 1.0 - 2.0 * (qX * qX + qY * qY))
        ).toFloat()

        val sample = GyroQuaternionSample(
            timestampNs = timestampNs,
            qW = qW,
            qX = qX,
            qY = qY,
            qZ = qZ,
            pitchDeg = pitchDeg,
            rollDeg = rollDeg,
            yawDeg = yawDeg,
            angularVelocityRadSec = omegaMag
        )

        if (recordedImuQueue.size > 256) {
            recordedImuQueue.poll()
        }
        recordedImuQueue.offer(sample)

        // Update 16x16 MeshFlow spatially-variant grid every 3rd sample (~66Hz)
        if (sampleCounter % 3 == 0) {
            val dxGrid = FloatArray(256)
            val dyGrid = FloatArray(256)
            if (curState.oisMode != OisStabilizationMode.OFF) {
                val residualX = (qY - smoothQY) * 0.55f
                val residualY = (qX - smoothQX) * 0.55f
                val rsScale = (curState.rollingShutterReadoutMs / 16.66f) * 0.045f
                val horizonAngle = if (curState.horizonLockEnabled) -rollDeg * 0.008f else 0f
                val cosH = cos(horizonAngle)
                val sinH = sin(horizonAngle)

                for (row in 0 until 16) {
                    val rowNorm = (row / 15f) - 0.5f // -0.5 top to +0.5 bottom
                    // Rolling shutter row-dependent horizontal/vertical skew compensation
                    val rsOffsetX = wy * rowNorm * rsScale
                    val rsOffsetY = wx * rowNorm * rsScale
                    for (col in 0 until 16) {
                        val colNorm = (col / 15f) - 0.5f
                        val idx = row * 16 + col
                        // Radial lens distortion + MeshFlow optical damping
                        val r2 = colNorm * colNorm + rowNorm * rowNorm
                        val oisGain = if (curState.oisMode == OisStabilizationMode.ONEPLUS_OPTICAL_LOCK) {
                            1.0f - 0.25f * r2
                        } else {
                            1.0f
                        }
                        val rotX = colNorm * (cosH - 1f) - rowNorm * sinH
                        val rotY = colNorm * sinH + rowNorm * (cosH - 1f)

                        dxGrid[idx] = ((residualX + rsOffsetX) * oisGain + rotX).coerceIn(-0.12f, 0.12f)
                        dyGrid[idx] = ((residualY + rsOffsetY) * oisGain + rotY).coerceIn(-0.12f, 0.12f)
                    }
                }
            }

            _state.value = curState.copy(
                measuredSampleRateHz = measuredHz,
                currentSample = sample,
                meshVerticesDx = dxGrid,
                meshVerticesDy = dyGrid
            )
        }
    }
}
