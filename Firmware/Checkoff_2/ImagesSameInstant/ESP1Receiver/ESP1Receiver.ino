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
    Serial.println("- Synchronized capture mode enabled");
    
    // Wait a bit for everything to stabilize, then send first capture command
    delay(2000);
    Serial.println("\n>>> Sending FIRST capture trigger <<<\n");
    Comms::sendCaptureCommand();
}

void loop() {
    static unsigned long now, lastTerminalTime = 0;
    static unsigned long lastCaptureTime = 0;

    now = millis();

    // Handle incoming UDP packets (image chunks)
    Comms::handleUDPPackets();

    // Drop stale/incomplete images if timeout AND trigger recovery
    if (buf1.img_id && (millis() - buf1.lastUpdate) > IMAGE_TIMEOUT_MS) {
        Serial.printf("Timeout: dropping incomplete image %u from camera 1\n", buf1.img_id);
        buf1.reset();
        
        // If both cameras timed out, trigger recovery
        if (buf2.img_id == 0 || (millis() - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) {
            buf2.reset();
            Serial.println("!!! Both cameras timed out - triggering recovery capture !!!");
            delay(500);
            Comms::sendCaptureCommand();
            lastCaptureTime = millis();
        }
    }
    if (buf2.img_id && (millis() - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) {
        Serial.printf("Timeout: dropping incomplete image %u from camera 2\n", buf2.img_id);
        buf2.reset();
        
        // If both cameras timed out, trigger recovery
        if (buf1.img_id == 0) {
            Serial.println("!!! Both cameras timed out - triggering recovery capture !!!");
            delay(500);
            Comms::sendCaptureCommand();
            lastCaptureTime = millis();
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