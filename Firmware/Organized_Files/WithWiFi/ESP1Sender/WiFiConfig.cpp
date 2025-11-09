#include "WiFiConfig.h"

// Definitions of globals
const char* WIFI_SSID = "GL-MT3000-8d3";
const char* WIFI_PASSWORD = "QYAXW83ASS";

IPAddress local_IP(192,168,8,101);
IPAddress gateway(192,168,8,1);
IPAddress subnet(255,255,255,0);

IPAddress receiver_IP(192,168,8,102);
const uint16_t receiver_port = 5000;

WiFiClient tcpClient;
