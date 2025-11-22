#include <Arduino.h>
#include "Comms.h"
#include "Sensors.h"
#include "ImageBuffer.h"

// Timing
unsigned long lastTriggerTime = 0;
unsigned long lastSensorSend = 0;
const unsigned long TRIGGER_INTERVAL = 50;
const unsigned long SENSOR_SEND_INTERVAL = 100;  // ⭐ Send sensors at 10Hz

void setup() {
    Serial.begin(115200);

    Comms::initWiFi();
    Comms::initUDPServer();
    Comms::initESPNow();
    Comms::registerCallback();

    Serial.println("Receiver initialized:");
    Serial.println("- UDP Server on port 8888 for images");
    Serial.println("- ESP-NOW active for triggering senders");
    Serial.println("- Serial for Jetson communication");
}

void loop() {
    static unsigned long now = millis();
    now = millis();

    // ⭐ PRIORITY 1: Handle incoming commands from Jetson
    Comms::handleJetsonSerial();

    // PRIORITY 2: Handle incoming UDP packets for images
    Comms::handleUDPPackets();

    // ⭐ PRIORITY 3: Send sensor data to Jetson periodically
    if (now - lastSensorSend >= SENSOR_SEND_INTERVAL) {
        Comms::sendSensorDataToJetson();
        lastSensorSend = now;
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

        Comms::sendTriggerToSenders();
        lastTriggerTime = now;
    }
}