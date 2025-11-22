#include "ImageBuffer.h"
#include "Sensors.h"
#include "Comms.h"
#include <Arduino.h>
#include <esp_now.h>
#include <stdlib.h>

// ---------- Sender MAC Addresses ----------
uint8_t sender1MAC[6] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};
uint8_t sender2MAC[6] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C}; // 14:33:5c:0a:48:2c

// ---------- Image Buffers ----------
ImageBuffer buf1;
ImageBuffer buf2;

// ---------- FPS Tracking ----------
unsigned long lastTimeCam1 = 0;
unsigned long lastTimeCam2 = 0;

// ---------- ImageBuffer Methods ----------
void ImageBuffer::reset() {
    if (chunks) {
        for (uint16_t i = 0; i < total_chunks; ++i) {
            if (chunks[i].data) free(chunks[i].data);
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
    if (total > MAX_CHUNKS) return false;
    chunks = (ChunkEntry*)calloc(total, sizeof(ChunkEntry));
    if (!chunks) return false;
    total_chunks = total;
    received_chunks = 0;
    lastUpdate = millis();
    return true;
}

// ---------- Finalize image ----------
void finalizeImage(ImageBuffer &ib) {
    unsigned long now = millis();
    float fps = 0;

    if (ib.camera_id == 1) {
        if (lastTimeCam1 > 0) fps = 1000.0 / (now - lastTimeCam1);
        lastTimeCam1 = now;
    } else if (ib.camera_id == 2) {
        if (lastTimeCam2 > 0) fps = 1000.0 / (now - lastTimeCam2);
        lastTimeCam2 = now;
    }

    size_t totalSize = 0;
    for (uint16_t i = 0; i < ib.total_chunks; i++) {
        totalSize += ib.chunks[i].len;
    }

    Serial.printf("Camera %u - Image %u complete (%u bytes, %u chunks) - FPS: %.2f\n", 
                  ib.camera_id, ib.img_id, totalSize, ib.total_chunks, fps);

    // Forward complete image to laptop
    Comms::forwardImageToLaptop(ib);

    ib.reset();
}

bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    for (int i = 0; i < 6; i++)                             // Loop through each byte of the MAC address
        if (mac1[i] != mac2[i]) return false;               // Return false if any byte differs
    return true;                                             // MAC addresses match
}

// ---------- ESP-NOW Callback ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!data || len <= 0) return;

    const uint8_t *mac = info->src_addr;

    if (compareMAC(mac, sender1MAC) && len == sizeof(SensorPacket1)) {
        memcpy(&packet1, data, sizeof(SensorPacket1));
        // ⭐ SEND TO LAPTOP VIA SERIAL
        Serial.print("\nSENSOR1,");
        Serial.print(packet1.sensor1); Serial.print(",");
        Serial.print(packet1.sensor2); Serial.print(",");
        Serial.print(packet1.sensor3); Serial.print(",");
        Serial.print(packet1.accelX, 3); Serial.print(",");
        Serial.print(packet1.accelY, 3); Serial.print(",");
        Serial.print(packet1.accelZ, 3); Serial.print(",");
        Serial.print(packet1.gyroX, 3); Serial.print(",");
        Serial.print(packet1.gyroY, 3); Serial.print(",");
        Serial.println(packet1.gyroZ, 3);
    }
    else if (compareMAC(mac, sender2MAC) && len == sizeof(SensorPacket2)) {
        memcpy(&packet2, data, sizeof(SensorPacket2));
        // ⭐ SEND TO LAPTOP VIA SERIAL
        Serial.print("SENSOR2,");
        Serial.print(packet2.sensor4); Serial.print(",");
        Serial.print(packet2.sensor5); Serial.print(",");
        Serial.print(packet2.sensor6); Serial.print(",");
    }

    // Compare MAC addresses (sensor packets or other triggers)
    bool fromSender1 = true;
    bool fromSender2 = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != sender1MAC[i]) fromSender1 = false;
        if (mac[i] != sender2MAC[i]) fromSender2 = false;
    }

    // Currently only trigger handling; sensor handling can be added here
    if ((fromSender1 || fromSender2) && len == 1 && data[0] == 1) {
        Serial.printf("Trigger/ACK received from %s\n", fromSender1 ? "Sender1" : "Sender2");
    }
}