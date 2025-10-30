#include "Comms.h"

namespace Comms {

static uint8_t* g_receiverMAC;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

void initESPNow(uint8_t receiverMAC[6]) {
    g_receiverMAC = receiverMAC;
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        while(1);
    }
    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer.");
        while(1);
    }
    Serial.println("ESP-NOW initialized.");
}

void sendImagePacket(uint8_t *payload, size_t payloadLen, uint16_t img_id, uint16_t seq, uint16_t total) {
    uint8_t packet[200];
    packet[0] = 'I';
    packet[1] = img_id & 0xFF;
    packet[2] = (img_id >> 8) & 0xFF;
    packet[3] = seq & 0xFF;
    packet[4] = (seq >> 8) & 0xFF;
    packet[5] = total & 0xFF;
    packet[6] = (total >> 8) & 0xFF;

    memcpy(&packet[7], payload, payloadLen);
    esp_now_send(g_receiverMAC, packet, 7 + payloadLen);
}

void sendImageDone(uint16_t img_id, uint16_t total) {
    uint8_t buf[5];
    buf[0] = 'D';
    buf[1] = img_id & 0xFF;
    buf[2] = (img_id >> 8) & 0xFF;
    buf[3] = total & 0xFF;
    buf[4] = (total >> 8) & 0xFF;
    esp_now_send(g_receiverMAC, buf, sizeof(buf));
}

void sendSensorData(SensorPacket data) {
    esp_now_send(g_receiverMAC, (uint8_t*)&data, sizeof(data));
}

} // namespace Comms
