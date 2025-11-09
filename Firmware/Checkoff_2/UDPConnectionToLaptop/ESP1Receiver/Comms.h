#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Sensors.h"
#include "ImageBuffer.h"

// ========== WiFi Configuration ==========
#define WIFI_SSID "GL-MT3000-8d3"
#define WIFI_PASSWORD "QYAXW83ASS"
#define IMAGE_PORT 8888

// ========== Laptop Configuration ==========
extern const char* LAPTOP_IP;   // Laptop's static IP (defined in Comms.cpp)
extern const int LAPTOP_PORT;   // Port laptop is listening on (defined in Comms.cpp)

// ========== Global Objects ==========
extern WiFiUDP udpServer;

namespace Comms {
    // WiFi Functions
    void initWiFi();                // Initialize WiFi connection
    
    // UDP Functions
    void initUDPServer();           // Initialize UDP server for images
    void handleUDPPackets();        // Handle incoming UDP packets
    
    // ESP-NOW Functions
    void initESPNow();              // Initialize ESP-NOW for sensor data
    void registerCallback();        // Register ESP-NOW receive callback
}

#endif  // COMMS_H