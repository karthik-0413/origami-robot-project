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

extern WiFiUDP udpServer;

namespace Comms {
    void initWiFi();
    void initUDPServer();
    void handleUDPPackets();
    void initESPNow();
    void registerCallback();
    void sendTriggerToSenders();
}

#endif
