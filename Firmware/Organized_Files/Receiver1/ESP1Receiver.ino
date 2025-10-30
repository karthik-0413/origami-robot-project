#include <Arduino.h>
#include "Comms.h"
#include "Sensors.h"
#include "ImageBuffer.h"

void setup() {
    Serial.begin(115200);
    Comms::initESPNow();
    Comms::registerCallback();

    Serial.println("Receiver initialized and ready for multiple senders.");
}

void loop() {
    static unsigned long now, lastTerminalTime = 0, lastPlotTime = 0;
    static bool plotIR = true;  // toggle between IR sensors and IMU

    now = millis();

    // ---------- Drop stale images roughly every second ----------
    if (now % 1000 < 50) {
        if (buf1.img_id && (now - buf1.lastUpdate) > IMAGE_TIMEOUT_MS) {
            Serial.printf("Timeout: dropping incomplete image %u from sender1\n", buf1.img_id);
            buf1.reset();
        }
        if (buf2.img_id && (now - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) {
            Serial.printf("Timeout: dropping incomplete image %u from sender2\n", buf2.img_id);
            buf2.reset();
        }
    }

    // ---------- Serial Plotter for IR sensors or IMU ----------
    if (now - lastPlotTime >= 100) { // plot every 100ms
        if (plotIR) {
            Serial.print("S1:"); Serial.print(packet1.sensor1);
            Serial.print(" S2:"); Serial.print(packet1.sensor2);
            Serial.print(" S3:"); Serial.print(packet1.sensor3);
            Serial.print(" S4:"); Serial.print(packet1.sensor4);
            Serial.print(" S5:"); Serial.print(packet2.sensor5);
            Serial.print(" S6:"); Serial.print(packet2.sensor6);
            Serial.print(" S7:"); Serial.print(packet2.sensor7);
            Serial.print(" S8:"); Serial.println(packet2.sensor8);
        } else {
            Serial.print("AccX:"); Serial.print(packet1.accelX);
            Serial.print(" AccY:"); Serial.print(packet1.accelY);
            Serial.print(" AccZ:"); Serial.print(packet1.accelZ);
            Serial.print(" GryX:"); Serial.print(packet1.gyroX);
            Serial.print(" GryY:"); Serial.print(packet1.gyroY);
            Serial.print(" GryZ:"); Serial.println(packet1.gyroZ);
        }
        lastPlotTime = now;
    }

    // ---------- Serial Monitor for simple debug ----------
    if (now - lastTerminalTime >= 1000) lastTerminalTime = now;
}
