#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <Arduino.h>
#include <esp_now.h>

// ---------- Sender MAC Addresses ----------
extern uint8_t sender1MAC[6];
extern uint8_t sender2MAC[6];

// ---------- Image Buffering Constants ----------
#define IMAGE_TIMEOUT_MS   2000
#define MAX_CHUNKS         50

struct ChunkEntry {
    uint8_t *data;
    size_t len;
    ChunkEntry() : data(nullptr), len(0) {}
};

struct ImageBuffer {
    uint8_t camera_id = 0;
    uint16_t img_id = 0;
    uint16_t total_chunks = 0;
    uint16_t received_chunks = 0;
    ChunkEntry *chunks = nullptr;
    unsigned long lastUpdate = 0;

    void reset();
    bool allocChunks(uint16_t total);
};

// Buffers for each camera
extern ImageBuffer buf1;
extern ImageBuffer buf2;

// Finalize image (calculate FPS)
void finalizeImage(ImageBuffer &ib);

// ESP-NOW callback (sensor data / trigger)
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);

#endif // IMAGEBUFFER_H
