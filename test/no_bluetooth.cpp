#include "mbed.h"

// Defines Trigger and Echo pins of the Ultrasonic Sensor
DigitalOut trigPin(D6); 
DigitalIn echoPin(D3); 

// Servo control pin
PwmOut servoPin(D5);

// Variables for the duration and the distance
Timer timer;
long duration;

// Function for calculating the distance measured by the Ultrasonic sensor
int calculateDistance() {
    trigPin = 0;          // Ensure trigger is low
    wait_us(2);           // Wait for a short period
    trigPin = 1;          // Trigger high for 10µs
    wait_us(10);          
    trigPin = 0;          // Trigger low

    // Wait for the echo to start
    while (!echoPin);

    // Start timing when echo starts
    timer.reset();
    timer.start();

    // Wait for the echo to end
    while (echoPin);

    timer.stop();

    // Calculate duration in microseconds
    duration = timer.elapsed_time().count(); 

    // Calculate distance: speed of sound is ~343 m/s or 0.0343 cm/µs
    int distance = duration * 0.0343 / 2;

    return distance; // Distance in cm
}



int main() {
    // Configure servo PWM: 20ms period and initial position
    servoPin.period_ms(20); // 20ms period for standard servos
    servoPin.pulsewidth_us(1500); // Neutral position (1.5ms pulse)
    int distance;
    int startDeg = 0;
    int stopDeg = 180;

    printf("Ultrasonic Sensor and Servo Test\n");

    while (true) {
        // Rotate the servo motor from 0 to 180 degrees
        for (int i = startDeg; i <= stopDeg; i++) {
            float pulseWidth = 1000 + (i * 1000.0 / 180.0); // Map degrees to pulse width (1ms to 2ms)
            servoPin.pulsewidth_us(pulseWidth);
            thread_sleep_for(30); // 30ms delay

            // Measure distance
            distance = calculateDistance();
            printf("Angle: %d, Distance: %d cm\n", i, distance);
        }

        // Rotate back from 180 to 0 degrees
        for (int i = stopDeg; i >= startDeg; i--) {
            float pulseWidth = 1000 + (i * 1000.0 / 180.0);
            servoPin.pulsewidth_us(pulseWidth);
            thread_sleep_for(30);

            // Measure distance
            distance = calculateDistance();
            printf("Angle: %d, Distance: %d cm\n", i, distance);
        }
    }
}

