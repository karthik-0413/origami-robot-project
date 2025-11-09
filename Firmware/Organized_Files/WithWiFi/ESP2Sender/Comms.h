#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Sensors.h"

// ESP-NOW receiver MAC
extern uint8_t receiverMAC[];

void initESPNow();
void initWiFiTCP();                   // New function to init Wi-Fi TCP
void sendSensorData();                 // Send IR & IMU data via ESP-NOW

#endif  // COMMS_H
