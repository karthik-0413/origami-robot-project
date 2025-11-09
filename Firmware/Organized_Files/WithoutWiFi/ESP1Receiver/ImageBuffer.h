#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <Arduino.h>
#include <esp_now.h>

// ---------- MAC Addresses ----------
extern uint8_t sender1MAC[6];    // Sender 1 MAC address
extern uint8_t sender2MAC[6];    // Sender 2 MAC address

// ---------- Image Buffering Constants ----------
#define CHUNK_SIZE 200                 // Total size of each chunk to send over ESP-NOW
#define IMG_HEADER_SIZE 7              // Number of bytes reserved for image header in each chunk
#define MAX_CHUNK_PAYLOAD (CHUNK_SIZE - IMG_HEADER_SIZE) // Maximum data bytes in each chunk after header
#define IMAGE_TIMEOUT_MS   5000        // Timeout (ms) to consider an image transmission as failed

// ---------- Chunk Entry Structure ----------
struct ChunkEntry {
    uint8_t *data;  // Pointer to chunk data
    size_t len;     // Length of the chunk
    ChunkEntry() : data(nullptr), len(0) {}  // Default constructor initializes empty chunk
};

// ---------- Image Buffer Structure ----------
struct ImageBuffer {
    uint16_t img_id = 0;           // Image ID
    uint16_t total_chunks = 0;     // Total number of chunks expected
    uint16_t received_chunks = 0;  // Number of chunks received so far
    ChunkEntry *chunks = nullptr;  // Array of chunk entries
    unsigned long lastUpdate = 0;  // Timestamp of last received chunk

    void reset();                  // Reset the buffer to initial empty state
    bool allocChunks(uint16_t total_chunks); // Allocate memory for storing chunks
};

// ---------- Buffers for each sender ----------
extern ImageBuffer buf1;   // Buffer for Sender 1
extern ImageBuffer buf2;   // Buffer for Sender 2

// ---------- Helper Functions ----------
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2);  // Compare two MAC addresses
String macToHex(const uint8_t *mac);                        // Convert MAC address to hexadecimal string
void tryFinalizeImage(ImageBuffer &ib, const uint8_t *senderMac); // Attempt to reconstruct the full image if all chunks are received

// ---------- ESP-NOW Callback ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len); // Called when an ESP-NOW packet is received

#endif  // IMAGEBUFFER_H
