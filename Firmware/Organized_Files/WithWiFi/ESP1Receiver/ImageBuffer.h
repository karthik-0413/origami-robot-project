#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <Arduino.h>
#include <esp_now.h>

// ---------- Sender MAC Addresses ----------
extern uint8_t sender1MAC[6];
extern uint8_t sender2MAC[6];

// ---------- ImageBuffer for TCP image ID tracking ----------
struct ImageBuffer {
    uint16_t img_id = 0;
    void reset();
};

// ---------- Buffers ----------
extern ImageBuffer tcpBuffer1;
extern ImageBuffer tcpBuffer2;

// ---------- TCP image handling ----------
void tryFinalizeImageTCP(uint8_t* data, size_t length, int clientIndex);

// ---------- ESP-NOW sensor packet handler ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);

#endif
