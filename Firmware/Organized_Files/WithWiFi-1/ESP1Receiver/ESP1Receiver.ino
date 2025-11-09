#include <Arduino.h>
#include "Comms.h"
#include "ImageBuffer.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n=== RELIABLE SYNCHRONIZED CAPTURE MODE ===");
    
    Comms::initWiFi();
    Comms::initUDPServer();
    Comms::initESPNow();
    Comms::registerCallback();

    Serial.println("System ready!");
    Serial.println("Waiting 3 seconds before first trigger...\n");
    
    delay(3000);
    Serial.println(">>> Sending FIRST trigger <<<\n");
    Comms::sendCaptureCommand();
}

void loop() {
    Comms::handleUDPPackets();

    // Generous timeout - 5 seconds
    unsigned long now = millis();
    
    if (buf1.img_id && (now - buf1.lastUpdate) > 5000) {
        Serial.printf("! Cam1 timeout (img %u: %u/%u chunks)\n", 
                      buf1.img_id, buf1.received_chunks, buf1.total_chunks);
        buf1.reset();
        checkAndTriggerRecovery();
    }
    
    if (buf2.img_id && (now - buf2.lastUpdate) > 5000) {
        Serial.printf("! Cam2 timeout (img %u: %u/%u chunks)\n",
                      buf2.img_id, buf2.received_chunks, buf2.total_chunks);
        buf2.reset();
        checkAndTriggerRecovery();
    }
}

void checkAndTriggerRecovery() {
    if (buf1.img_id == 0 && buf2.img_id == 0) {
        Serial.println("\n>>> Recovery trigger <<<\n");
        delay(500);
        Comms::sendCaptureCommand();
    }
}