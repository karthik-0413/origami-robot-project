#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Sensors.h"

#define WIFI_SSID "GL-MT3000-8d3"
// #define WIFI_SSID "designlab"
#define WIFI_PASSWORD "QYAXW83ASS"
// #define WIFI_PASSWORD "designlab1"
#define IMAGE_PORT 8888
#define CAMERA_ID 2

extern uint8_t receiverMAC[];
extern WiFiUDP udpClient;

void initWiFi();
bool sendImageUDP(uint8_t* imageData, uint32_t length);
void initESPNow();
void sendSensorData();

#endif
