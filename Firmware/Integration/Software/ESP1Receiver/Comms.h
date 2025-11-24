#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>

#define WIFI_SSID "GL-MT3000-264"
#define WIFI_PASSWORD "goodlife"
#define IMAGE_PORT 8888
#define LAPTOP_IP "192.168.8.104"
#define LAPTOP_PORT 9999

namespace Comms {
    extern uint8_t sender1MAC[];
    extern uint8_t sender2MAC[];

    void initWiFi();
    void initUDPServer();
    void initESPNow();
    void registerCallback();
    void handleUDPPackets();
    void sendTriggerToSenders();
    void forwardImageToLaptop(class ImageBuffer &);
    
    // ⭐ NEW: Jetson communication functions
    void handleJetsonSerial();        // Read commands from Jetson
    void sendSensorDataToJetson();    // Send sensor data to Jetson
    void forwardPositionToSenders();  // Send position cmd to both senders
    void forwardHingeToSender(uint8_t hingeID);  // Send hinge cmd to specific sender
    unsigned long parseTimestamp(String timestamp_str);  // ⭐ NEW
}

#endif