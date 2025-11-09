#include <Arduino.h>
#include "Comms.h"
#include "Sensors.h"
#include "ImageBuffer.h"

void setup() {
    Serial.begin(115200);                     // Initialize serial for debug/plotting
    Comms::initESPNow();                      // Initialize ESP-NOW communication
    Comms::registerCallback();                // Register receive callback for incoming data

    Serial.println("Receiver initialized and ready for multiple senders."); // Debug
}

void loop() {
    static unsigned long now, lastTerminalTime = 0, lastPlotTime = 0;

    now = millis();                           // Current time in milliseconds

    // ---------- Drop stale/incomplete images roughly every second ----------
    if (now % 1000 < 50) {                    // Check roughly once per second
        if (buf1.img_id && (now - buf1.lastUpdate) > IMAGE_TIMEOUT_MS) {
            Serial.printf("Timeout: dropping incomplete image %u from sender1\n", buf1.img_id);
            buf1.reset();                     // Clear buffer for sender1
        }
        if (buf2.img_id && (now - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) {
            Serial.printf("Timeout: dropping incomplete image %u from sender2\n", buf2.img_id);
            buf2.reset();                     // Clear buffer for sender2
        }
    }

    // static bool plotIR = true;               // Toggle between IR sensors and IMU data
    
    // // ---------- Serial Plotter output for IR sensors or IMU ----------
    // if (now - lastPlotTime >= 100) {          // Update plot every 100ms
    //     if (plotIR) {                         // Plot IR/digital sensors
    //         Serial.print("S1:"); Serial.print(packet1.sensor1);     // Sensor 1
    //         Serial.print(" S2:"); Serial.print(packet1.sensor2);    // Sensor 2
    //         Serial.print(" S3:"); Serial.print(packet1.sensor3);    // Sensor 3
    //         Serial.print(" S4:"); Serial.print(packet1.sensor4);    // Sensor 4
    //         Serial.print(" S5:"); Serial.print(packet2.sensor5);    // Sensor 5
    //         Serial.print(" S6:"); Serial.print(packet2.sensor6);    // Sensor 6
    //         Serial.print(" S7:"); Serial.print(packet2.sensor7);    // Sensor 7
    //         Serial.print(" S8:"); Serial.println(packet2.sensor8);  // Sensor 8
    //     } else {                              // Plot IMU data
    //         Serial.print("AccX:"); Serial.print(packet1.accelX);
    //         Serial.print(" AccY:"); Serial.print(packet1.accelY);
    //         Serial.print(" AccZ:"); Serial.print(packet1.accelZ);
    //         Serial.print(" GryX:"); Serial.print(packet1.gyroX);
    //         Serial.print(" GryY:"); Serial.print(packet1.gyroY);
    //         Serial.print(" GryZ:"); Serial.println(packet1.gyroZ);
    //     }
    //     lastPlotTime = now;                   // Update last plot time
    // }

    // ---------- Serial Monitor simple debug ----------
    if (now - lastTerminalTime >= 1000) {    // Placeholder for future debug printing
        lastTerminalTime = now;
    }
}
