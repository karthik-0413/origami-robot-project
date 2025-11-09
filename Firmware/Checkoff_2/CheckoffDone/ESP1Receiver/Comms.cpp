#include "Comms.h"
#include "ImageBuffer.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>

WiFiUDP udpServer;
WiFiUDP udpClient; // For forwarding to laptop

namespace Comms {

uint8_t sender1MAC[] = {0x14,0x33,0x5C,0x02,0x88,0x34};
uint8_t sender2MAC[] = {0x38,0x18,0x2B,0xB2,0x23,0x64};

void initWiFi() {
    WiFi.mode(WIFI_AP_STA);
    IPAddress local_IP(192,168,8,103);
    IPAddress gateway(192,168,8,1);
    IPAddress subnet(255,255,255,0);
    IPAddress dns(192,168,8,1);
    WiFi.config(local_IP,gateway,subnet,dns);
    WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
    while(WiFi.status()!=WL_CONNECTED) delay(500);
}

void initUDPServer(){ udpServer.begin(IMAGE_PORT); }

void handleUDPPackets() {
    int packetSize = udpServer.parsePacket();
    if(packetSize==0) return;

    uint8_t packet[1500];
    int len = udpServer.read(packet,sizeof(packet));
    if(len<5) return;

    uint8_t camera_id = packet[0];
    uint16_t seq = ((uint16_t)packet[1]<<8)|packet[2];
    uint16_t total = ((uint16_t)packet[3]<<8)|packet[4];
    const uint8_t *payload = &packet[5];
    int payloadLen = len-5;

    ImageBuffer *ib = (camera_id==1)?&buf1:&buf2;
    if(ib->img_id==0 || ib->total_chunks==0){
        if(!ib->allocChunks(total)) return;
        ib->camera_id=camera_id;
        ib->img_id++;
    }

    if(seq>=ib->total_chunks) return;
    if(ib->chunks[seq].data) { ib->lastUpdate=millis(); return; }

    ib->chunks[seq].data=(uint8_t*)malloc(payloadLen);
    memcpy(ib->chunks[seq].data,payload,payloadLen);
    ib->chunks[seq].len=payloadLen;
    ib->received_chunks++;
    ib->lastUpdate=millis();

    if(ib->received_chunks==ib->total_chunks){
        finalizeImage(*ib);
        if(buf1.received_chunks==buf1.total_chunks && buf2.received_chunks==buf2.total_chunks){
            sendTriggerToSenders();
        }
    }
}

void initESPNow() {
    if(esp_now_init()!=ESP_OK){ while(1); }
    esp_now_peer_info_t peer1={},peer2={};
    memcpy(peer1.peer_addr,sender1MAC,6);
    peer1.channel=0; peer1.encrypt=false; esp_now_add_peer(&peer1);
    memcpy(peer2.peer_addr,sender2MAC,6);
    peer2.channel=0; peer2.encrypt=false; esp_now_add_peer(&peer2);
}

void registerCallback(){ esp_now_register_recv_cb(OnDataRecv); }

void sendTriggerToSenders() {
    uint8_t triggerMsg=0x01;
    esp_now_send(sender1MAC,&triggerMsg,1);
    esp_now_send(sender2MAC,&triggerMsg,1);
}

// Forward complete image to laptop
void forwardImageToLaptop(ImageBuffer &ib) {
    // Calculate total image size
    size_t totalSize = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        totalSize += ib.chunks[i].len;
    }
    
    // Allocate buffer for complete image
    uint8_t *imageData = (uint8_t*)malloc(totalSize);
    if (!imageData) {
        Serial.println("Failed to allocate memory for image forwarding");
        return;
    }
    
    // Concatenate all chunks
    size_t offset = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        memcpy(imageData + offset, ib.chunks[i].data, ib.chunks[i].len);
        offset += ib.chunks[i].len;
    }
    
    // Send image to laptop via UDP
    // Format: [camera_id][img_size_high][img_size_low][image_data]
    const size_t HEADER_SIZE = 3;
    const size_t MAX_UDP_SIZE = 1400;
    const size_t MAX_PAYLOAD = MAX_UDP_SIZE - HEADER_SIZE;
    
    uint16_t numPackets = (totalSize + MAX_PAYLOAD - 1) / MAX_PAYLOAD;
    
    for (uint16_t pkt = 0; pkt < numPackets; pkt++) {
        size_t chunkStart = pkt * MAX_PAYLOAD;
        size_t chunkSize = min((size_t)MAX_PAYLOAD, totalSize - chunkStart);
        
        uint8_t packet[MAX_UDP_SIZE];
        packet[0] = ib.camera_id;
        packet[1] = (totalSize >> 8) & 0xFF;
        packet[2] = totalSize & 0xFF;
        memcpy(packet + HEADER_SIZE, imageData + chunkStart, chunkSize);
        
        udpClient.beginPacket(LAPTOP_IP, LAPTOP_PORT);
        udpClient.write(packet, HEADER_SIZE + chunkSize);
        udpClient.endPacket();
        
        delay(2); // Small delay to prevent overwhelming the network
    }
    
    free(imageData);
    Serial.printf("Forwarded Camera %u image (%u bytes) to laptop\n", ib.camera_id, totalSize);
}

} // namespace Comms