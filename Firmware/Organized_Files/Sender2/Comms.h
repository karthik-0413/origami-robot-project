#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Sensors.h"

namespace Comms {
    void initESPNow(uint8_t receiverMAC[6]);
    void sendImagePacket(uint8_t *payload, size_t payloadLen, uint16_t img_id, uint16_t seq, uint16_t total);
    void sendImageDone(uint16_t img_id, uint16_t total);
    void sendSensorData(SensorPacket data);
}

#endif
