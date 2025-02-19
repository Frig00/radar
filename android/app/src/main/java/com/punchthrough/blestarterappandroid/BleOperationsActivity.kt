/*
 * Copyright 2024 Punch Through Design LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.punchthrough.blestarterappandroid

import android.app.Activity
import android.app.AlertDialog
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCharacteristic
import android.content.Context
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.punchthrough.blestarterappandroid.ble.ConnectionEventListener
import com.punchthrough.blestarterappandroid.ble.ConnectionManager
import com.punchthrough.blestarterappandroid.ble.ConnectionManager.parcelableExtraCompat
import com.punchthrough.blestarterappandroid.databinding.ActivityBleOperationsBinding
import timber.log.Timber
import java.util.Locale
import java.util.UUID
import kotlinx.coroutines.*

class BleOperationsActivity : AppCompatActivity() {

    private lateinit var binding: ActivityBleOperationsBinding
    private val device: BluetoothDevice by lazy {
        intent.parcelableExtraCompat(BluetoothDevice.EXTRA_DEVICE)
            ?: error("Missing BluetoothDevice from MainActivity!")
    }
    private val distanceCharacteristicUUID = UUID.fromString("0a924ca7-87cd-4699-a3bd-abdcd9cf126a")
    private val angleCharacteristicUUID = UUID.fromString("485f4145-52b9-4644-af1f-7a6b9322490f")
    private val runningCharacteristicUUID = UUID.fromString("8dd6a1b7-bc75-4741-8a26-264af75807de")
    private val thresholdCharacteristicUUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
//    private val pollingScope = CoroutineScope(Dispatchers.Default + Job())
//    private var isPolling = false
    private var isMotorRunning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ConnectionManager.registerListener(connectionEventListener)

        binding = ActivityBleOperationsBinding.inflate(layoutInflater)
        setContentView(binding.root)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowTitleEnabled(true)
            title = getString(R.string.ble_playground)
        }

        readInitialRunningState()
//        readAndSubscribeCharacteristics()
        setupToggleButton()
//        startPolling()

        setupThresholdSlider()

        enableCharacteristicNotifications()
    }

    private fun enableCharacteristicNotifications() {
        val gattServiceList = ConnectionManager.servicesOnDevice(device) ?: return
        gattServiceList.forEach { gattService ->
            gattService.characteristics.forEach { characteristic ->
                when (characteristic.uuid) {
                    distanceCharacteristicUUID, angleCharacteristicUUID -> {
                        Timber.d("BleOperationsActivity - Enabling notifications for characteristic: ${characteristic.uuid}") // ADD THIS LOG
                        ConnectionManager.enableNotifications(device, characteristic)
                    }
                }
            }
        }
    }

//    private fun startPolling() {
//        isPolling = true
//        pollingScope.launch {
//            while (isPolling) {
//                if (isMotorRunning) {  // Only poll when motor is running
//                    val gattServiceList = ConnectionManager.servicesOnDevice(device)
//                    gattServiceList?.forEach { gattService ->
//                        gattService.characteristics.forEach { characteristic ->
//                            when (characteristic.uuid) {
//                                distanceCharacteristicUUID, angleCharacteristicUUID -> {
//                                    ConnectionManager.readCharacteristic(device, characteristic)
//                                }
//                            }
//                        }
//                    }
//                }
//                delay(100) // Increased delay to 50ms
//            }
//        }
//    }

    private fun readInitialRunningState() {
        val gattServiceList = ConnectionManager.servicesOnDevice(device) ?: return
        gattServiceList.forEach { gattService ->
            gattService.characteristics.forEach { characteristic ->
                if (characteristic.uuid == runningCharacteristicUUID) {
                    ConnectionManager.readCharacteristic(device, characteristic)
                }
            }
        }
    }

//    private fun readAndSubscribeCharacteristics() {
//        // This function is now only used for initial setup, no subscriptions needed
//        val gattServiceList = ConnectionManager.servicesOnDevice(device) ?: return
//        gattServiceList.forEach { gattService ->
//            gattService.characteristics.forEach { characteristic ->
//                when (characteristic.uuid) {
//                    distanceCharacteristicUUID, angleCharacteristicUUID -> {
//                        ConnectionManager.readCharacteristic(device, characteristic)
//                    }
//                }
//            }
//        }
//    }

    private fun setupToggleButton() {
        binding.toggleMotorButton.setOnClickListener {
            isMotorRunning = !isMotorRunning
            writeRunningCharacteristic(isMotorRunning)
            binding.toggleMotorButton.text = if (isMotorRunning) "Stop Motor" else "Start Motor"
        }
    }

    private fun setupThresholdSlider() {
        // Write initial threshold value
        writeThresholdCharacteristic(15)
        
        binding.thresholdSlider.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                binding.thresholdLabel.text = "Threshold: $progress cm"
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {}

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
                seekBar?.let { 
                    writeThresholdCharacteristic(it.progress)
                }
            }
        })
    }

    private fun writeRunningCharacteristic(running: Boolean) {
        val gattServiceList = ConnectionManager.servicesOnDevice(device) ?: return
        gattServiceList.forEach { gattService ->
            gattService.characteristics.forEach { characteristic ->
                if (characteristic.uuid == runningCharacteristicUUID) {
                    val value = if (running) 0x01.toByte() else 0x00.toByte()
                    ConnectionManager.writeCharacteristic(device, characteristic, byteArrayOf(value))
                }
            }
        }
    }

    private fun writeThresholdCharacteristic(threshold: Int) {
        val gattServiceList = ConnectionManager.servicesOnDevice(device) ?: return
        gattServiceList.forEach { gattService ->
            gattService.characteristics.forEach { characteristic ->
                if (characteristic.uuid == thresholdCharacteristicUUID) {
                    // Convert to single byte (uint8_t)
                    val value = threshold.toByte()
                    Timber.i("Threshold value set to: ${value}")
                    Timber.i("Size threshold: ${byteArrayOf(value).count()}")

                    ConnectionManager.writeCharacteristic(device, characteristic, byteArrayOf(value))
                }
            }
        }
    }

    override fun onDestroy() {
//        isPolling = false
//        pollingScope.cancel()
        ConnectionManager.unregisterListener(connectionEventListener)
        ConnectionManager.teardownConnection(device)
        super.onDestroy()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            android.R.id.home -> {
                onBackPressed()
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }

    private val connectionEventListener = ConnectionEventListener().apply {
        onDisconnect = {
            runOnUiThread {
                AlertDialog.Builder(this@BleOperationsActivity)
                    .setTitle("Disconnected")
                    .setMessage("Disconnected from device.")
                    .setPositiveButton("OK") { _, _ -> onBackPressed() }
                    .show()
            }
        }

        onCharacteristicChanged = { _, characteristic, value -> // Handle notifications here
            when (characteristic.uuid) {
                distanceCharacteristicUUID -> {
                    val distance = value.toInt() // Assuming the value is in the correct format
                    runOnUiThread {
                        binding.distanceLabel.text = "Distance: $distance cm"
                        updateRadarView()
                    }
                }
                angleCharacteristicUUID -> {
                    val angle = value.toInt() // Assuming the value is in the correct format
                    runOnUiThread {
                        binding.angleLabel.text = "Angle: $angle°"
                        updateRadarView()
                    }
                }
                runningCharacteristicUUID -> {
                    val running = value[0] != 0x00.toByte()
                    Timber.i("Running read (notification): ${value.toHexString()}") // Log as notification
                    isMotorRunning = running
                    runOnUiThread {
                        binding.toggleMotorButton.text = if (running) "Stop Motor" else "Start Motor"
                    }
                }
            }
            Timber.i("Notification from ${characteristic.uuid}: ${value.toHexString()}") // Log as notification
        }

        onCharacteristicRead = { _, characteristic, value -> // Keep handling initial read of running state
            if (characteristic.uuid == runningCharacteristicUUID) {
                val running = value[0] != 0x00.toByte()
                Timber.i("Running initial read: ${value.toHexString()}") // Log as initial read
                isMotorRunning = running
                runOnUiThread {
                    binding.toggleMotorButton.text = if (running) "Stop Motor" else "Start Motor"
                }
            } else {
                Timber.i("Read from ${characteristic.uuid}: ${value.toHexString()}") // Keep other read logging
            }
        }
    }

    // Add helper method to convert hex string to int
    private fun ByteArray.toInt(): Int {
        return this.toHexString().toInt(16)
    }

    // Add helper method to update RadarView
    private fun updateRadarView() {
        val angle = binding.angleLabel.text.toString()
            .removePrefix("Angle: ")
            .removeSuffix("°")
            .toIntOrNull() ?: 0

        val distance = binding.distanceLabel.text.toString()
            .removePrefix("Distance: ")
            .removeSuffix(" cm")
            .toIntOrNull() ?: 0

        binding.radarView.updateData(angle, distance)
    }

    private fun String.hexToBytes() =
        this.chunked(2).map { it.uppercase(Locale.US).toInt(16).toByte() }.toByteArray()

    private fun ByteArray.toHexString() = joinToString("") { "%02x".format(it) }
}