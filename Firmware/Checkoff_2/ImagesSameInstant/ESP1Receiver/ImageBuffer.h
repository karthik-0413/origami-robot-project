#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <Arduino.h>
#include <esp_now.h>

// ---------- MAC Addresses (for ESP-NOW sensor data only) ----------
extern uint8_t sender1MAC[6];    // Sender 1 MAC address
extern uint8_t sender2MAC[6];    // Sender 2 MAC address

// ---------- Image Buffering Constants ----------
#define IMAGE_TIMEOUT_MS   2000        // Timeout (ms) to consider an image transmission as failed
#define MAX_CHUNKS         50          // Maximum chunks per image

// ---------- Chunk Entry Structure ----------
struct ChunkEntry {
    uint8_t *data;  // Pointer to chunk data
    size_t len;     // Length of the chunk
    ChunkEntry() : data(nullptr), len(0) {}
};

// ---------- Image Buffer Structure ----------
struct ImageBuffer {
    uint8_t camera_id = 0;         // Camera ID (1 or 2)
    uint16_t img_id = 0;           // Image ID counter
    uint16_t total_chunks = 0;     // Total number of chunks expected
    uint16_t received_chunks = 0;  // Number of chunks received so far
    ChunkEntry *chunks = nullptr;  // Array of chunk entries
    unsigned long lastUpdate = 0;  // Timestamp of last received chunk

    void reset();                  // Reset the buffer to initial empty state
    bool allocChunks(uint16_t total); // Allocate memory for storing chunks
};

// ---------- Buffers for each camera ----------
extern ImageBuffer buf1;   // Buffer for Camera 1
extern ImageBuffer buf2;   // Buffer for Camera 2

// ---------- Helper Functions ----------
void finalizeImage(ImageBuffer &ib);  // Process complete image and calculate FPS
void triggerNextCapture();            // Trigger next synchronized capture

// ---------- ESP-NOW Callback (for sensor data only) ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);

#endif  // IMAGEBUFFER_H