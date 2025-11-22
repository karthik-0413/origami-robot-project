#include <Arduino.h>
#include "Comms.h"
#include "Sensors.h"
#include "ImageBuffer.h"

// ---------- Timing ----------
unsigned long lastTriggerTime = 0;
const unsigned long TRIGGER_INTERVAL = 50; // ms, small delay after image cycle

void setup() {
    Serial.begin(115200);

    Comms::initWiFi();         // Initialize WiFi
    Comms::initUDPServer();    // Start UDP server
    Comms::initESPNow();       // Initialize ESP-NOW
    Comms::registerCallback(); // Register sensor data / trigger callback

    Serial.println("Receiver initialized:");
    Serial.println("- UDP Server on port 8888 for images");
    Serial.println("- ESP-NOW active for triggering senders");
}

void loop() {
    static unsigned long now, lastTerminalTime = 0, lastPlotTime = 0;

    now = millis();                           // Current time in milliseconds

    // Handle incoming UDP packets for images
    Comms::handleUDPPackets();

    static bool plotIR = true;               // Toggle between IR sensors and IMU data
    
    // ---------- Serial Plotter output for IR sensors or IMU ----------
    if (now - lastPlotTime >= 100) {          // Update plot every 100ms
        if (plotIR) {                         // Plot IR/digital sensors
            // Serial.print("S1:"); Serial.print(packet1.sensor1);     // Sensor 1
            // Serial.print(" S2:"); Serial.print(packet1.sensor2);    // Sensor 2
            // Serial.print(" S3:"); Serial.print(packet1.sensor3);    // Sensor 3
            // Serial.print(" S4:"); Serial.print(packet2.sensor4);    // Sensor 4
            // Serial.print(" S5:"); Serial.print(packet2.sensor5);    // Sensor 5
            // Serial.print(" S6:"); Serial.print(packet2.sensor6);    // Sensor 6
        } else {                              // Plot IMU data
            Serial.print("AccX:"); Serial.print(packet1.accelX);
            Serial.print(" AccY:"); Serial.print(packet1.accelY);
            Serial.print(" AccZ:"); Serial.print(packet1.accelZ);
            Serial.print(" GryX:"); Serial.print(packet1.gyroX);
            Serial.print(" GryY:"); Serial.print(packet1.gyroY);
            Serial.print(" GryZ:"); Serial.println(packet1.gyroZ);
        }
        lastPlotTime = now;                   // Update last plot time
    }

    // Drop stale images
    if (now % 1000 < 50) {
        if (buf1.img_id && (now - buf1.lastUpdate) > IMAGE_TIMEOUT_MS) buf1.reset();
        if (buf2.img_id && (now - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) buf2.reset();
    }

    // Send trigger when ready for next pair of images
    if ((buf1.img_id == 0 || buf1.received_chunks == buf1.total_chunks) &&
        (buf2.img_id == 0 || buf2.received_chunks == buf2.total_chunks) &&
        now - lastTriggerTime > TRIGGER_INTERVAL) {

        uint8_t triggerMsg = 1;

        esp_now_send(sender1MAC, &triggerMsg, 1);
        esp_now_send(sender2MAC, &triggerMsg, 1);

        lastTriggerTime = now;
        // Serial.println("Trigger sent to both senders");
    }

    // Optional: periodic status
    static unsigned long lastStatus = 0;
    if (now - lastStatus >= 5000) {
        // Serial.printf("Status - Cam1: img_id=%u, chunks=%u/%u | Cam2: img_id=%u, chunks=%u/%u\n", 
        //               buf1.img_id, buf1.received_chunks, buf1.total_chunks,
        //               buf2.img_id, buf2.received_chunks, buf2.total_chunks);
        lastStatus = now;
    }
}
