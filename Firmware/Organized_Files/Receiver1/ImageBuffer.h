#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <Arduino.h>
#include <esp_now.h>

// ---------- MAC Addresses ----------
extern uint8_t sender1MAC[6];
extern uint8_t sender2MAC[6];

// ---------- Image buffering ----------
#define MAX_CHUNK_PAYLOAD  (200 - 7)
#define IMAGE_TIMEOUT_MS   5000

struct ChunkEntry {
    uint8_t *data;
    size_t len;
    ChunkEntry() : data(nullptr), len(0) {}
};

struct ImageBuffer {
    uint16_t img_id = 0;
    uint16_t total_chunks = 0;
    uint16_t received_chunks = 0;
    ChunkEntry *chunks = nullptr;
    unsigned long lastUpdate = 0;

    void reset();
    bool allocChunks(uint16_t total_chunks);
};

// one buffer per sender
extern ImageBuffer buf1;
extern ImageBuffer buf2;

// ---------- Helpers ----------
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2);
String macToHex(const uint8_t *mac);
void tryFinalizeImage(ImageBuffer &ib, const uint8_t *senderMac);

// ---------- ESP-NOW callback ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);

#endif
