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
    unsigned long now = millis();

    // Handle incoming UDP packets for images
    Comms::handleUDPPackets();

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
        Serial.println("Trigger sent to both senders");
    }

    // Optional: periodic status
    static unsigned long lastStatus = 0;
    if (now - lastStatus >= 5000) {
        Serial.printf("Status - Cam1: img_id=%u, chunks=%u/%u | Cam2: img_id=%u, chunks=%u/%u\n", 
                      buf1.img_id, buf1.received_chunks, buf1.total_chunks,
                      buf2.img_id, buf2.received_chunks, buf2.total_chunks);
        lastStatus = now;
    }
}
