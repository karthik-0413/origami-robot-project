#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Sensors.h"

// ========== WiFi Configuration ==========
// Wi-Fi credentials for GL-MT3000 router
#define WIFI_SSID "GL-MT3000-8d3"
#define WIFI_PASSWORD "QYAXW83ASS"

// Receiver configuration for UDP image transfer
#define RECEIVER_IP "192.168.8.103"
#define IMAGE_PORT 8888

// Camera ID - CHANGE THIS TO 2 FOR SECOND ESP!
// Camera 1 will get IP 192.168.8.101
// Camera 2 will get IP 192.168.8.102
#define CAMERA_ID 2

// ========== ESP-NOW Configuration ==========
// MAC address of the receiver ESP device for ESP-NOW sensor data
extern uint8_t receiverMAC[];

// ========== Global Objects ==========
extern WiFiUDP udpClient;

// ========== Function Declarations ==========

// WiFi Functions
void initWiFi();                                   // Initialize WiFi connection to router

// UDP Functions  
bool sendImageUDP(uint8_t* imageData, uint32_t length);  // Send image via UDP

// ESP-NOW Functions
void initESPNow();                                 // Initialize ESP-NOW for sensor data
void sendSensorData();                             // Send IR & IMU sensor data via ESP-NOW

#endif  // COMMS_H