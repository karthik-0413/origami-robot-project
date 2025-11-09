#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "Sensors.h"
#include "ImageBuffer.h"

namespace Comms {
    void initESPNow();                // Initializes ESP-NOW communication
    void registerCallback();          // Registers ESP-NOW receive callback

    void initWiFiTCP(uint16_t port);  // Initializes TCP server to receive images
    void handleTCPClients();          // Handles incoming TCP connections and image reception
}

#endif  // COMMS_H
