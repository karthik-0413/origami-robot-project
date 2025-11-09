#include "Comms.h"
#include "WiFiConfig.h"   // Include this instead of declaring externs manually


uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

void initESPNow() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) while(1);

    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) while(1);
}

void initWiFiTCP() {
    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected");

    while (!tcpClient.connect(receiver_IP, receiver_port)) {
        Serial.println("Connecting to TCP receiver...");
        delay(500);
    }
    Serial.println("TCP connected to receiver");
}

void sendSensorData() {
    esp_now_send(receiverMAC, (uint8_t*)&sensorData, sizeof(sensorData));
}
