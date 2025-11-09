#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>

// Wi-Fi credentials
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// Static IP configuration
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;

// Receiver TCP info
extern IPAddress receiver_IP;
extern const uint16_t receiver_port;

// TCP client
extern WiFiClient tcpClient;

#endif
