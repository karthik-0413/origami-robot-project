#include <Arduino.h>
#include "Comms.h"
#include "Sensors.h"
#include "ImageBuffer.h"

void setup() {
    Serial.begin(115200);

    // ---------- ESP-NOW for sensors ----------
    Comms::initESPNow();
    Comms::registerCallback();

    // ---------- TCP server for images ----------
    Comms::initWiFiTCP(5000);

    Serial.println("Receiver ready: ESP-NOW sensors + TCP images");
}

void loop() {
    static unsigned long now, lastPlotTime = 0;
    now = millis();

    // ---------- Handle incoming TCP images ----------
    Comms::handleTCPClients();

    // ---------- Plot IR sensors or IMU ----------
    if (now - lastPlotTime >= 100) {
        bool plotIR = false;  // toggle as needed
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
}
