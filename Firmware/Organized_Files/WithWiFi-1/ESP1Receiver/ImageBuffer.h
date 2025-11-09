#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <Arduino.h>

#define MAX_CHUNKS 40  // Increased for smaller chunk size

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

extern ImageBuffer buf1;
extern ImageBuffer buf2;

void finalizeImage(ImageBuffer &ib);
void triggerNextCapture();
void checkAndTriggerRecovery();

#endif