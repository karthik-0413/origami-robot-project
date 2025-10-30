#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Sensors.h"

extern uint8_t receiverMAC[];

void initESPNow();
bool sendImagePacketWithHeader(uint8_t *payload, size_t payloadLen, uint16_t img_id, uint16_t seq, uint16_t total);
void sendImageDone(uint16_t img_id, uint16_t total);
void sendSensorData();

#endif
