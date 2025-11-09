#include <Arduino.h>
#include "Comms.h"
#include "ImageBuffer.h"

void setup() {
    Serial.begin(115200);
    Comms::initWiFi();
    Comms::initUDPServer();
    Comms::initESPNow();
    Comms::registerCallback();

    // Send first trigger to start capture
    Comms::sendTriggerToSenders();
}

void loop() {
    Comms::handleUDPPackets();
}
