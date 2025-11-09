#include "ImageBuffer.h"
#include "Sensors.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFiUdp.h>
#include <stdlib.h>

// Forward declarations
extern WiFiUDP forwardClient;
extern uint8_t sender1MAC[6];  // Declared in Comms.cpp
extern uint8_t sender2MAC[6];  // Declared in Comms.cpp

// Forward declare function from Comms
namespace Comms {
    void sendCaptureCommand();
}

// ---------- Image Buffers ----------
ImageBuffer buf1;  // Buffer for camera 1
ImageBuffer buf2;  // Buffer for camera 2

// ---------- FPS Tracking ----------
unsigned long lastTimeCam1 = 0;
unsigned long lastTimeCam2 = 0;

// ---------- Synchronization ----------
bool cam1Ready = false;
bool cam2Ready = false;

// Define triggerNextCapture here
void triggerNextCapture() {
    delay(100);  // Brief pause before next capture
    Comms::sendCaptureCommand();
}

/****************************************************
 * Function: ImageBuffer::reset
 * Description: Frees all allocated memory for chunks
 *              and resets the buffer state.
 ****************************************************/
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

/****************************************************
 * Function: ImageBuffer::allocChunks
 * Description: Allocates memory for storing chunks.
 ****************************************************/
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

/****************************************************
 * Function: forwardImage
 * Description: Forwards a single camera image to laptop.
 ****************************************************/
void forwardImage(ImageBuffer &ib) {
    // Calculate total image size
    size_t totalSize = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        totalSize += ib.chunks[i].len;
    }

    // Reassemble image into single buffer
    uint8_t *imageData = (uint8_t*)malloc(totalSize);
    if (!imageData) {
        Serial.println("Failed to allocate image buffer");
        return;
    }
    
    size_t offset = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        if (ib.chunks[i].data && ib.chunks[i].len > 0) {
            memcpy(&imageData[offset], ib.chunks[i].data, ib.chunks[i].len);
            offset += ib.chunks[i].len;
        }
    }
    
    // Forward to laptop in UDP chunks
    const size_t UDP_CHUNK = 1400;
    const size_t HEADER_SIZE = 9;
    uint16_t totalChunks = (totalSize + UDP_CHUNK - 1) / UDP_CHUNK;
    
    extern const char* LAPTOP_IP;
    
    for (uint16_t seq = 0; seq < totalChunks; seq++) {
        size_t chunkOffset = seq * UDP_CHUNK;
        size_t chunkSize = min((size_t)UDP_CHUNK, (size_t)(totalSize - chunkOffset));
        
        uint8_t packet[HEADER_SIZE + UDP_CHUNK];
        size_t pktOffset = 0;
        
        // Start marker
        packet[pktOffset++] = 0xFF;
        packet[pktOffset++] = 0xD8;
        packet[pktOffset++] = 0xFF;
        packet[pktOffset++] = 0xAA;
        
        // Camera ID
        packet[pktOffset++] = ib.camera_id;
        
        // Chunk info: seq(2) + total(2)
        packet[pktOffset++] = (seq >> 8) & 0xFF;
        packet[pktOffset++] = seq & 0xFF;
        packet[pktOffset++] = (totalChunks >> 8) & 0xFF;
        packet[pktOffset++] = (totalChunks & 0xFF);
        
        // Copy chunk data
        memcpy(&packet[pktOffset], &imageData[chunkOffset], chunkSize);
        pktOffset += chunkSize;
        
        // Send UDP packet
        forwardClient.beginPacket(LAPTOP_IP, 9999);
        forwardClient.write(packet, pktOffset);
        forwardClient.endPacket();
    }
    
    // Send completion marker
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
    
    Serial.printf("Camera %u - Forwarded %u bytes (%u chunks)\n", 
                  ib.camera_id, totalSize, totalChunks);
}

/****************************************************
 * Function: finalizeImage
 * Description: Marks image as ready and forwards when both cameras ready.
 ****************************************************/
void finalizeImage(ImageBuffer &ib) {
    unsigned long now = millis();
    float fps = 0;

    // Calculate FPS
    if (ib.camera_id == 1) {
        if (lastTimeCam1 > 0) fps = 1000.0 / (now - lastTimeCam1);
        lastTimeCam1 = now;
        cam1Ready = true;
    } else if (ib.camera_id == 2) {
        if (lastTimeCam2 > 0) fps = 1000.0 / (now - lastTimeCam2);
        lastTimeCam2 = now;
        cam2Ready = true;
    }

    Serial.printf("Camera %u ready - FPS: %.2f\n", ib.camera_id, fps);

    // Check if both cameras are ready
    if (cam1Ready && cam2Ready) {
        // Forward in order: Camera 1 first, then Camera 2
        Serial.println("=== Both cameras ready - forwarding synchronized pair ===");
        
        forwardImage(buf1);
        delay(50);  // Small gap to ensure ordered delivery
        forwardImage(buf2);
        
        // Reset both buffers
        buf1.reset();
        buf2.reset();
        
        // Clear ready flags
        cam1Ready = false;
        cam2Ready = false;
        
        Serial.println("=== Pair forwarded ===\n");
        
        // Trigger next capture now that pair is complete
        triggerNextCapture();
    } else {
        // Still waiting for other camera
        if (cam1Ready) {
            Serial.println("Waiting for Camera 2...");
        } else {
            Serial.println("Waiting for Camera 1...");
        }
    }
}

/****************************************************
 * Function: OnDataRecv
 * Description: ESP-NOW callback for receiving SENSOR DATA ONLY.
 *              Images now come via UDP, not ESP-NOW.
 ****************************************************/
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t *mac = info->src_addr;
    
    if (len <= 0) return;

    // Compare MAC addresses
    bool isSender1 = true;
    bool isSender2 = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != sender1MAC[i]) isSender1 = false;
        if (mac[i] != sender2MAC[i]) isSender2 = false;
    }

    // Handle sensor data from sender 1
    if (isSender1 && len == sizeof(SensorPacket1)) {
        memcpy(&packet1, data, sizeof(SensorPacket1));
    }
    // Handle sensor data from sender 2
    else if (isSender2 && len == sizeof(SensorPacket2)) {
        memcpy(&packet2, data, sizeof(SensorPacket2));
    }
}