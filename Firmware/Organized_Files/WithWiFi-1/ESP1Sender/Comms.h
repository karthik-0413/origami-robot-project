#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "CameraModule.h"

// WiFi Configuration
#define WIFI_SSID "GL-MT3000-8d3"
#define WIFI_PASSWORD "QYAXW83ASS"

// Receiver configuration
#define RECEIVER_IP "192.168.8.103"
#define IMAGE_PORT 8888

// Camera ID - CHANGE THIS TO 2 FOR SECOND ESP!
#define CAMERA_ID 1

// ESP-NOW Configuration
extern uint8_t receiverMAC[];

// Global Objects
extern WiFiUDP udpClient;

// Function Declarations
void initWiFi();
bool sendImageUDP(uint8_t* imageData, uint32_t length);
void initESPNow();

#endif