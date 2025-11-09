#include "Comms.h"

WiFiUDP udpServer;
WiFiUDP forwardClient;

const char* LAPTOP_IP = "192.168.8.100";
const int LAPTOP_PORT = 9999;

uint8_t sender1MAC[6] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};
uint8_t sender2MAC[6] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64};

namespace Comms {

void initWiFi() {
    WiFi.mode(WIFI_AP_STA);
    
    IPAddress local_IP(192, 168, 8, 103);
    IPAddress gateway(192, 168, 8, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(192, 168, 8, 1);
    
    WiFi.config(local_IP, gateway, subnet, dns);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi OK - IP: %s\n", WiFi.localIP().toString().c_str());
    }
}

void initUDPServer() {
    udpServer.begin(IMAGE_PORT);
    Serial.printf("UDP Server: port %d\n", IMAGE_PORT);
}

void handleUDPPackets() {
    int packetSize = udpServer.parsePacket();
    if (packetSize == 0) return;
    
    uint8_t packet[1500];
    int len = udpServer.read(packet, sizeof(packet));
    
    if (len < 5) return;
    
    uint8_t camera_id = packet[0];
    uint16_t seq = ((uint16_t)packet[1] << 8) | packet[2];
    uint16_t total = ((uint16_t)packet[3] << 8) | packet[4];
    const uint8_t *payload = &packet[5];
    int payloadLen = len - 5;
    
    if (camera_id != 1 && camera_id != 2) return;
    
    ImageBuffer *ib = (camera_id == 1) ? &buf1 : &buf2;
    
    if (ib->img_id == 0 || ib->total_chunks == 0) {
        if (!ib->allocChunks(total)) return;
        ib->camera_id = camera_id;
        ib->img_id++;
    }
    
    if (seq >= ib->total_chunks) return;
    if (ib->chunks[seq].data != nullptr) {
        ib->lastUpdate = millis();
        return;
    }
    
    ib->chunks[seq].data = (uint8_t*)malloc(payloadLen);
    if (!ib->chunks[seq].data) {
        ib->reset();
        return;
    }
    
    memcpy(ib->chunks[seq].data, payload, payloadLen);
    ib->chunks[seq].len = payloadLen;
    ib->received_chunks++;
    ib->lastUpdate = millis();
    
    if (ib->received_chunks == ib->total_chunks) {
        finalizeImage(*ib);
    }
}

void initESPNow() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        while (1);
    }
    
    esp_now_peer_info_t peer1 = {};
    memcpy(peer1.peer_addr, sender1MAC, 6);
    peer1.channel = 0;
    peer1.encrypt = false;
    esp_now_add_peer(&peer1);
    
    esp_now_peer_info_t peer2 = {};
    memcpy(peer2.peer_addr, sender2MAC, 6);
    peer2.channel = 0;
    peer2.encrypt = false;
    esp_now_add_peer(&peer2);
    
    Serial.println("ESP-NOW ready");
}

void registerCallback() {
    // No callback needed for receiver in camera-only mode
}

void sendCaptureCommand() {
    uint8_t command = 0xC4;
    esp_now_send(sender1MAC, &command, 1);
    esp_now_send(sender2MAC, &command, 1);
}

}  // namespace Comms