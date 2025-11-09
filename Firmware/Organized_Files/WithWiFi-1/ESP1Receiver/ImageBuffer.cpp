#include "ImageBuffer.h"
#include <WiFiUdp.h>
#include <stdlib.h>

extern WiFiUDP forwardClient;
extern uint8_t sender1MAC[6];
extern uint8_t sender2MAC[6];

namespace Comms {
    void sendCaptureCommand();
}

ImageBuffer buf1;
ImageBuffer buf2;

unsigned long lastTimeCam1 = 0;
unsigned long lastTimeCam2 = 0;
bool cam1Ready = false;
bool cam2Ready = false;

void triggerNextCapture() {
    delay(100);  // Small pause before next trigger
    Comms::sendCaptureCommand();
}

void ImageBuffer::reset() {
    if (chunks) {
        for (uint16_t i = 0; i < total_chunks; ++i) {
            if (chunks[i].data) {
                free(chunks[i].data);
                chunks[i].data = nullptr;
            }
        }
        free(chunks);
        chunks = nullptr;
    }
    camera_id = 0;
    img_id = 0;
    total_chunks = 0;
    received_chunks = 0;
    lastUpdate = 0;
}

bool ImageBuffer::allocChunks(uint16_t total) {
    reset();
    
    if (total > MAX_CHUNKS) {
        Serial.printf("Too many chunks: %u (max %u)\n", total, MAX_CHUNKS);
        return false;
    }
    
    chunks = (ChunkEntry*)calloc(total, sizeof(ChunkEntry));
    if (!chunks) {
        Serial.println("Failed to allocate chunks array");
        return false;
    }
    
    total_chunks = total;
    received_chunks = 0;
    lastUpdate = millis();
    return true;
}

void forwardImage(ImageBuffer &ib) {
    size_t totalSize = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        totalSize += ib.chunks[i].len;
    }

    Serial.printf("  Forwarding Cam%u: %u bytes (%u chunks)\n", 
                  ib.camera_id, totalSize, ib.total_chunks);

    uint8_t *imageData = (uint8_t*)malloc(totalSize);
    if (!imageData) {
        Serial.println("  Forward malloc failed!");
        return;
    }
    
    size_t offset = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        if (ib.chunks[i].data && ib.chunks[i].len > 0) {
            memcpy(&imageData[offset], ib.chunks[i].data, ib.chunks[i].len);
            offset += ib.chunks[i].len;
        }
    }
    
    const size_t UDP_CHUNK = 1400;
    const size_t HEADER_SIZE = 9;
    uint16_t totalChunks = (totalSize + UDP_CHUNK - 1) / UDP_CHUNK;
    
    extern const char* LAPTOP_IP;
    
    for (uint16_t seq = 0; seq < totalChunks; seq++) {
        size_t chunkOffset = seq * UDP_CHUNK;
        size_t chunkSize = min((size_t)UDP_CHUNK, (size_t)(totalSize - chunkOffset));
        
        uint8_t packet[HEADER_SIZE + UDP_CHUNK];
        size_t pktOffset = 0;
        
        packet[pktOffset++] = 0xFF;
        packet[pktOffset++] = 0xD8;
        packet[pktOffset++] = 0xFF;
        packet[pktOffset++] = 0xAA;
        packet[pktOffset++] = ib.camera_id;
        packet[pktOffset++] = (seq >> 8) & 0xFF;
        packet[pktOffset++] = seq & 0xFF;
        packet[pktOffset++] = (totalChunks >> 8) & 0xFF;
        packet[pktOffset++] = (totalChunks & 0xFF);
        
        memcpy(&packet[pktOffset], &imageData[chunkOffset], chunkSize);
        pktOffset += chunkSize;
        
        forwardClient.beginPacket(LAPTOP_IP, 9999);
        forwardClient.write(packet, pktOffset);
        forwardClient.endPacket();
        
        delay(2);  // Small delay for laptop reception
    }
    
    uint8_t donePacket[9];
    donePacket[0] = 0xFF;
    donePacket[1] = 0xD9;
    donePacket[2] = 0xFF;
    donePacket[3] = 0xBB;
    donePacket[4] = ib.camera_id;
    donePacket[5] = (totalChunks >> 8) & 0xFF;
    donePacket[6] = totalChunks & 0xFF;
    donePacket[7] = (totalSize >> 8) & 0xFF;
    donePacket[8] = totalSize & 0xFF;
    
    forwardClient.beginPacket(LAPTOP_IP, 9999);
    forwardClient.write(donePacket, 9);
    forwardClient.endPacket();
    
    free(imageData);
}

void finalizeImage(ImageBuffer &ib) {
    unsigned long now = millis();
    float fps = 0;

    if (ib.camera_id == 1) {
        if (lastTimeCam1 > 0) fps = 1000.0 / (now - lastTimeCam1);
        lastTimeCam1 = now;
        cam1Ready = true;
    } else if (ib.camera_id == 2) {
        if (lastTimeCam2 > 0) fps = 1000.0 / (now - lastTimeCam2);
        lastTimeCam2 = now;
        cam2Ready = true;
    }

    Serial.printf("Cam%u complete [%.1f FPS] - %u/%u chunks received\n", 
                  ib.camera_id, fps, ib.received_chunks, ib.total_chunks);

    if (cam1Ready && cam2Ready) {
        Serial.println("\n=== BOTH READY - Forwarding synchronized pair ===");
        
        // forwardImage(buf1);
        // forwardImage(buf2);
        
        buf1.reset();
        buf2.reset();
        
        cam1Ready = false;
        cam2Ready = false;
        
        Serial.println("=== Pair sent - triggering next ===\n");
        triggerNextCapture();
    } else {
        if (cam1Ready) {
            Serial.println("  -> Waiting for Cam2...");
        } else {
            Serial.println("  -> Waiting for Cam1...");
        }
    }
}