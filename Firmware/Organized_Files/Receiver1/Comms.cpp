#include "Comms.h"
#include "ImageBuffer.h"
#include <WiFi.h>
#include <esp_now.h>

namespace Comms {

void initESPNow() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        while (1);
    }
    Serial.println("ESP-NOW initialized.");
}

void registerCallback() {
    esp_now_register_recv_cb(OnDataRecv); // now works fine
}

} // namespace Comms
