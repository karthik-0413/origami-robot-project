#include <Arduino.h>
#include "Comms.h"
#include "Sensors.h"
#include "ImageBuffer.h"

void setup() {
    Serial.begin(115200);
    
    Comms::initWiFi();                        // Initialize WiFi
    Comms::initUDPServer();                   // Start UDP server on port 8888
    Comms::initESPNow();                      // Initialize ESP-NOW for sensor data
    Comms::registerCallback();                // Register ESP-NOW receive callback

    Serial.println("Receiver initialized:");
    Serial.println("- UDP Server on port 8888 for images");
    Serial.println("- ESP-NOW active for sensor data");
}

void loop() {
    static unsigned long now, lastTerminalTime = 0;

    now = millis();

    // Handle incoming UDP packets (image chunks)
    Comms::handleUDPPackets();

    // Drop stale/incomplete images roughly every second
    if (now % 1000 < 50) {
        if (buf1.img_id && (now - buf1.lastUpdate) > IMAGE_TIMEOUT_MS) {
            Serial.printf("Timeout: dropping incomplete image %u from camera 1\n", buf1.img_id);
            buf1.reset();
        }
        if (buf2.img_id && (now - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) {
            Serial.printf("Timeout: dropping incomplete image %u from camera 2\n", buf2.img_id);
            buf2.reset();
        }
    }

    // Optional: Periodic status updates
    if (now - lastTerminalTime >= 5000) {
        Serial.printf("Status - Cam1: img_id=%u, chunks=%u/%u | Cam2: img_id=%u, chunks=%u/%u\n", 
                      buf1.img_id, buf1.received_chunks, buf1.total_chunks,
                      buf2.img_id, buf2.received_chunks, buf2.total_chunks);
        lastTerminalTime = now;
    }
}