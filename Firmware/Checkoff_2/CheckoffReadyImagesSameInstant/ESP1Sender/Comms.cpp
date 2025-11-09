#include "Comms.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>

uint8_t receiverMAC[] = {0x14,0x33,0x5C,0x0A,0x48,0x2C};
WiFiUDP udpClient;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

void initWiFi() {
    WiFi.mode(WIFI_AP_STA);
    IPAddress local_IP(192,168,8,100+CAMERA_ID);
    IPAddress gateway(192,168,8,1);
    IPAddress subnet(255,255,255,0);
    IPAddress dns(192,168,8,1);
    WiFi.config(local_IP, gateway, subnet, dns);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while(WiFi.status() != WL_CONNECTED && attempts < 20){ delay(500); attempts++; }
    Serial.println(WiFi.localIP());
}

void initESPNow() {
    if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while(1); }
    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, receiverMAC, 6);
    peer.channel = 0; peer.encrypt=false;
    esp_now_add_peer(&peer);
}

bool sendImageUDP(uint8_t* imageData, uint32_t length) {
    const size_t UDP_PAYLOAD = 1400;
    const size_t HEADER_SIZE = 5;
    uint16_t totalChunks = (length + UDP_PAYLOAD - 1)/UDP_PAYLOAD;
    uint8_t *packet = (uint8_t*)malloc(UDP_PAYLOAD+HEADER_SIZE);
    if(!packet) return false;

    for(uint16_t seq=0; seq<totalChunks; seq++){
        size_t offset=seq*UDP_PAYLOAD;
        size_t chunkSize=min(UDP_PAYLOAD,(size_t)(length-offset));
        packet[0]=CAMERA_ID;
        packet[1]=(seq>>8)&0xFF; packet[2]=seq&0xFF;
        packet[3]=(totalChunks>>8)&0xFF; packet[4]=totalChunks&0xFF;
        memcpy(&packet[HEADER_SIZE],&imageData[offset],chunkSize);
        udpClient.beginPacket("192.168.8.103", IMAGE_PORT);
        udpClient.write(packet, HEADER_SIZE+chunkSize);
        udpClient.endPacket();
        yield();
    }
    free(packet);
    return true;
}
