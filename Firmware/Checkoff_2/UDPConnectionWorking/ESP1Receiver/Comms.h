#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "ImageBuffer.h"

#define WIFI_SSID "GL-MT3000-8d3"
#define WIFI_PASSWORD "QYAXW83ASS"
#define IMAGE_PORT 8888

// UDP forwarding to laptop
#define LAPTOP_IP "192.168.8.100"
#define LAPTOP_PORT 9999

extern WiFiUDP udpServer;
extern WiFiUDP udpClient;

namespace Comms {
    void initWiFi();
    void initUDPServer();
    void handleUDPPackets();
    void initESPNow();
    void registerCallback();
    void sendTriggerToSenders();
    void forwardImageToLaptop(ImageBuffer &ib);
}

#endif