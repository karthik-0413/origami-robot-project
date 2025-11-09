#include "ImageBuffer.h"
#include "Sensors.h"
#include <Arduino.h>
#include <esp_now.h>
#include <stdlib.h>

// ---------- Sender MAC Addresses (for ESP-NOW sensor data) ----------
uint8_t sender1MAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34}; // Sender 1
uint8_t sender2MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64}; // Sender 2

// ---------- Image Buffers ----------
ImageBuffer buf1;  // Buffer for camera 1
ImageBuffer buf2;  // Buffer for camera 2

// ---------- FPS Tracking ----------
unsigned long lastTimeCam1 = 0;
unsigned long lastTimeCam2 = 0;

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
 * Function: finalizeImage
 * Description: Processes complete image and sends via Serial.
 ****************************************************/
void finalizeImage(ImageBuffer &ib) {
    unsigned long now = millis();
    float fps = 0;

    // Calculate FPS based on camera
    if (ib.camera_id == 1) {
        if (lastTimeCam1 > 0) fps = 1000.0 / (now - lastTimeCam1);
        lastTimeCam1 = now;
    } else if (ib.camera_id == 2) {
        if (lastTimeCam2 > 0) fps = 1000.0 / (now - lastTimeCam2);
        lastTimeCam2 = now;
    }

    // Calculate total image size
    size_t totalSize = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        totalSize += ib.chunks[i].len;
    }

    Serial.printf("Camera %u - Image %u complete (%u bytes, %u chunks) - FPS: %.2f\n", 
                  ib.camera_id, ib.img_id, totalSize, ib.total_chunks, fps);

    // // Send image via Serial to computer/Jetson
    // // Format: <IMG_START:camera_id:image_size>\n
    // Serial.printf("<IMG_START:%u:%u>\n", ib.camera_id, totalSize);
    
    // // Send all chunks as binary data
    // for (uint16_t i = 0; i < ib.total_chunks; i++) {
    //     if (ib.chunks[i].data && ib.chunks[i].len > 0) {
    //         Serial.write(ib.chunks[i].data, ib.chunks[i].len);
    //     }
    // }
    
    // // End marker
    // Serial.print("\n<IMG_END>\n");

    ib.reset();
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