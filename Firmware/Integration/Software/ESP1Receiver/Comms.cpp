#include "Comms.h"
#include "ImageBuffer.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>

WiFiUDP udpServer;
WiFiUDP udpClient; // For forwarding to laptop

namespace Comms {

uint8_t sender1MAC[] = {0x14,0x33,0x5C,0x02,0x88,0x34}; // 14:33:5c:02:88:34
uint8_t sender2MAC[] = {0x14,0x33,0x5C,0x0A,0x48,0x2C}; // 14:33:5c:0a:48:2c

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
        // Check if BOTH cameras are complete BEFORE finalizing
        bool bothComplete = (buf1.received_chunks == buf1.total_chunks && 
                             buf2.received_chunks == buf2.total_chunks);
        
        // Finalize this image (forwards to laptop and resets buffer)
        finalizeImage(*ib);
        
        // Send trigger only if BOTH were complete
        if(bothComplete){
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
        
        // No delay - send as fast as possible
        yield(); // Just yield to prevent watchdog
    }
    
    free(imageData);
    Serial.printf("Forwarded Camera %u image (%u bytes) to laptop\n", ib.camera_id, totalSize);
}

// ============================================================================
// ⭐ NEW: JETSON SERIAL COMMUNICATION
// ============================================================================

void handleJetsonSerial() {
    // Read incoming commands from Jetson via Serial
    if (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        
        // Parse different command types
        // Format: "POS,lx,ly,lz,ax,ay,az" or "HINGE,id,angle"
        
        if (line.startsWith("POS,")) {
            // Position command
            // Format: POS,linear_x,linear_y,linear_z,angular_x,angular_y,angular_z
            int idx = 4; // Skip "POS,"
            
            int comma1 = line.indexOf(',', idx);
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            int comma4 = line.indexOf(',', comma3 + 1);
            int comma5 = line.indexOf(',', comma4 + 1);
            
            if (comma1 > 0 && comma2 > 0 && comma3 > 0 && comma4 > 0 && comma5 > 0) {
                positionCmd.linear_x = line.substring(idx, comma1).toFloat();
                positionCmd.linear_y = line.substring(comma1 + 1, comma2).toFloat();
                positionCmd.linear_z = line.substring(comma2 + 1, comma3).toFloat();
                positionCmd.angular_x = line.substring(comma3 + 1, comma4).toFloat();
                positionCmd.angular_y = line.substring(comma4 + 1, comma5).toFloat();
                positionCmd.angular_z = line.substring(comma5 + 1).toFloat();
                
                Serial.println("Position command received");
                
                // Forward to both senders
                forwardPositionToSenders();
            }
        }
        else if (line.startsWith("HINGE,")) {
            // Hinge command
            // Format: HINGE,id,angle
            int comma1 = line.indexOf(',', 6);
            
            if (comma1 > 0) {
                hingeCmd.hingeID = line.substring(6, comma1).toInt();
                hingeCmd.targetAngle = line.substring(comma1 + 1).toFloat();
                
                Serial.printf("Hinge command: ID=%d, Angle=%.2f\n", 
                             hingeCmd.hingeID, hingeCmd.targetAngle);
                
                // Forward to specific sender
                forwardHingeToSender(hingeCmd.hingeID);
            }
        }
    }
}

void sendSensorDataToJetson() {
    // Send sensor data back to Jetson
    // Format: SENSOR1,ir1,ir2,ir3,ax,ay,az,gx,gy,gz,hinge
    Serial.print("SENSOR1,");
    Serial.print(packet1.sensor1); Serial.print(",");
    Serial.print(packet1.sensor2); Serial.print(",");
    Serial.print(packet1.sensor3); Serial.print(",");
    Serial.print(packet1.accelX, 3); Serial.print(",");
    Serial.print(packet1.accelY, 3); Serial.print(",");
    Serial.print(packet1.accelZ, 3); Serial.print(",");
    Serial.print(packet1.gyroX, 3); Serial.print(",");
    Serial.print(packet1.gyroY, 3); Serial.print(",");
    Serial.print(packet1.gyroZ, 3); Serial.print(",");
    Serial.println(packet1.currentHingeAngle, 2);
    
    // Format: SENSOR2,ir4,ir5,ir6,hinge
    Serial.print("SENSOR2,");
    Serial.print(packet2.sensor4); Serial.print(",");
    Serial.print(packet2.sensor5); Serial.print(",");
    Serial.print(packet2.sensor6); Serial.print(",");
    Serial.println(packet2.currentHingeAngle, 2);
}

void forwardPositionToSenders() {
    // Send position command to BOTH senders via ESP-NOW
    
    // Create packet with identifier byte
    uint8_t posPacket[sizeof(PositionCommand) + 1];
    posPacket[0] = 0xAA;  // Position command identifier
    memcpy(&posPacket[1], &positionCmd, sizeof(PositionCommand));
    
    // Send to both senders
    esp_err_t result1 = esp_now_send(sender1MAC, posPacket, sizeof(posPacket));
    esp_err_t result2 = esp_now_send(sender2MAC, posPacket, sizeof(posPacket));
    
    if (result1 == ESP_OK && result2 == ESP_OK) {
        Serial.println("Position cmd forwarded to both senders");
    } else {
        Serial.println("Position cmd forward failed");
    }
}

void forwardHingeToSender(uint8_t hingeID) {
    // Send hinge command to SPECIFIC sender via ESP-NOW
    
    // Create packet with identifier byte
    uint8_t hingePacket[sizeof(HingeCommand) + 1];
    hingePacket[0] = 0xBB;  // Hinge command identifier
    memcpy(&hingePacket[1], &hingeCmd, sizeof(HingeCommand));
    
    // Determine which sender to send to
    uint8_t *targetMAC = (hingeID == 1) ? sender1MAC : sender2MAC;
    
    esp_err_t result = esp_now_send(targetMAC, hingePacket, sizeof(hingePacket));
    
    if (result == ESP_OK) {
        Serial.printf("Hinge cmd forwarded to sender %d\n", hingeID);
    } else {
        Serial.println("Hinge cmd forward failed");
    }
}

} // namespace Comms