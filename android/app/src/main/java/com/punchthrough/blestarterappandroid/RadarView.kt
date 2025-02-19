/*
 * Copyright 2025 Punch Through Design LLC
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

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import kotlin.math.cos
import kotlin.math.sin

class RadarView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private var width = 0f
    private var height = 0f
    private var centerX = 0f
    private var centerY = 0f

    // Sample data (replace with real data later)
    private var currentAngle = 0
    private var currentDistance = 0

    // Replace measurementHistory list with a map
    private val measurements = mutableMapOf<Int, Int>() // angle -> distance
    
    private val radarPaint = Paint().apply {
        color = Color.rgb(98, 245, 31) // green color
        strokeWidth = 2f
        style = Paint.Style.STROKE
        isAntiAlias = true
    }

    private val objectPaint = Paint().apply {
        color = Color.rgb(255, 10, 10) // Solid red
        strokeWidth = 4f
        style = Paint.Style.FILL // Changed to FILL for area visualization
        isAntiAlias = true
    }

    private val trailPaint = Paint().apply {
        color = Color.rgb(255, 10, 10) // Solid red
        strokeWidth = 4f
        style = Paint.Style.FILL
        isAntiAlias = true
    }

    private val linePaint = Paint().apply {
        color = Color.rgb(30, 250, 60)
        strokeWidth = 9f
        style = Paint.Style.STROKE
        isAntiAlias = true
    }

    private val textPaint = Paint().apply {
        color = Color.rgb(98, 245, 31)
        textSize = 40f
        isAntiAlias = true
    }

    private val TRAIL_LENGTH = 30 // degrees to keep in history
    private val MAX_DISTANCE = 30 // maximum measurable distance in cm

    private var isClockwise = true
    private var lastAngle = -1

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        width = w.toFloat()
        height = h.toFloat()
        centerX = width / 2
        centerY = height - height * 0.074f
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // Draw background
        canvas.drawColor(Color.BLACK)

        // Draw radar arcs
        drawRadar(canvas)

        // Draw detected object first
        drawObject(canvas)

        // Draw scanning line last so it appears on top
        drawLine(canvas)

        // Draw text information
        drawText(canvas)
    }

    fun updateData(angle: Int, distance: Int) {
        currentAngle = angle
        currentDistance = distance
        
        // Update the measurement for this angle
        measurements[angle] = distance
        
        // Remove old measurements that are more than TRAIL_LENGTH degrees away
        val currentAngleNormalized = angle % 360
        measurements.keys.toList().forEach { storedAngle ->
            val storedAngleNormalized = storedAngle % 360
            val diff = calculateAngleDifference(currentAngleNormalized, storedAngleNormalized)
            if (diff > TRAIL_LENGTH) {
                measurements.remove(storedAngle)
            }
        }
        
        invalidate()
    }

    private fun calculateAngleDifference(angle1: Int, angle2: Int): Int {
        val diff = Math.abs(angle1 - angle2)
        return if (diff > 180) 360 - diff else diff
    }

    private fun getRadarRadius(): Float {
        return (width - width * 0.0625f) / 2
    }

    private fun drawRadar(canvas: Canvas) {
        val radarRadius = getRadarRadius()

        canvas.save()
        canvas.translate(centerX, centerY)

        // Draw arcs
        val rect = RectF(-radarRadius, -radarRadius, radarRadius, radarRadius)
        canvas.drawArc(rect, 180f, 180f, false, radarPaint)

        // Draw lines
        canvas.drawLine(-width/2, 0f, width/2, 0f, radarPaint)

        // Draw angle lines
        for (angle in arrayOf(30, 60, 90, 120, 150)) {
            val radians = Math.toRadians(angle.toDouble())
            val cos = cos(radians).toFloat()
            val sin = sin(radians).toFloat()
            canvas.drawLine(0f, 0f, (-width/2) * cos, (-width/2) * sin, radarPaint)
        }

        canvas.restore()
    }

    private fun drawLine(canvas: Canvas) {
        canvas.save()
        canvas.translate(centerX, centerY)

        val radians = Math.toRadians(currentAngle.toDouble())
        val lineLength = height - height * 0.12f
        val endX = lineLength * cos(radians).toFloat()
        val endY = -lineLength * sin(radians).toFloat()

        canvas.drawLine(0f, 0f, endX, endY, linePaint)
        canvas.restore()
    }

    private fun drawObject(canvas: Canvas) {
        canvas.save()
        canvas.translate(centerX, centerY)

        val maxRadius = getRadarRadius()
        val currentAngleNormalized = currentAngle % 360

        measurements.forEach { (angle, distance) ->
            val storedAngleNormalized = angle % 360
            val diff = calculateAngleDifference(currentAngleNormalized, storedAngleNormalized)
            
            if (diff <= TRAIL_LENGTH) {
                val radians = Math.toRadians(angle.toDouble())
                // Clamp distance to MAX_DISTANCE
                val clampedDistance = distance.coerceAtMost(MAX_DISTANCE)
                val scaledDistance = (clampedDistance.toFloat() / MAX_DISTANCE.toFloat()) * maxRadius
                
                val startX = scaledDistance * cos(radians).toFloat()
                val startY = -scaledDistance * sin(radians).toFloat()
                
                canvas.drawPath(android.graphics.Path().apply {
                    moveTo(startX, startY)
                    
                    val endX = maxRadius * cos(radians).toFloat()
                    val endY = -maxRadius * sin(radians).toFloat()
                    lineTo(endX, endY)
                    
                    val angleWidth = 3f
                    val radiansWidth = Math.toRadians((angle + angleWidth).toDouble())
                    val endX2 = maxRadius * cos(radiansWidth).toFloat()
                    val endY2 = -maxRadius * sin(radiansWidth).toFloat()
                    lineTo(endX2, endY2)
                    
                    val startX2 = scaledDistance * cos(radiansWidth).toFloat()
                    val startY2 = -scaledDistance * sin(radiansWidth).toFloat()
                    lineTo(startX2, startY2)
                    
                    close()
                }, trailPaint)
            }
        }

        canvas.restore()
    }

    private fun drawText(canvas: Canvas) {
        canvas.drawText("Angle: $currentAngle°", 50f, height - 50f, textPaint)
        canvas.drawText("Distance: $currentDistance cm", 50f, height - 100f, textPaint)
    }
}