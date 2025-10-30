#include <Arduino.h>
#include <Wire.h>
#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"

uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};

void setup() {
    Serial.begin(115200);
    Wire.begin();

    initCamera();
    initVL53L0X();
    Comms::initESPNow(receiverMAC);
}

void loop() {
    captureAndSend();
    readVL53L0X();
    sendSensorData();
}
