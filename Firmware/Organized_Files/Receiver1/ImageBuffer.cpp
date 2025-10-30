#include "ImageBuffer.h"
#include "Sensors.h"
#include <Arduino.h>
#include <esp_now.h>
#include <stdlib.h>

uint8_t sender1MAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C};
uint8_t sender2MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64};

ImageBuffer buf1;
ImageBuffer buf2;

// ---------- ImageBuffer methods ----------
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
    img_id = 0;
    total_chunks = 0;
    received_chunks = 0;
    lastUpdate = 0;
}

bool ImageBuffer::allocChunks(uint16_t total_chunks) {
    reset();
    chunks = (ChunkEntry*)calloc(total_chunks, sizeof(ChunkEntry));
    if (!chunks) return false;
    this->total_chunks = total_chunks;
    received_chunks = 0;
    lastUpdate = millis();
    return true;
}

// ---------- Helper functions ----------
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    for (int i = 0; i < 6; i++) if (mac1[i] != mac2[i]) return false;
    return true;
}

String macToHex(const uint8_t *mac) {
    char tmp[13];
    sprintf(tmp, "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(tmp);
}

void tryFinalizeImage(ImageBuffer &ib, const uint8_t *senderMac) {
    if (ib.img_id == 0 || ib.total_chunks == 0 || ib.received_chunks != ib.total_chunks) {
        Serial.printf("Incomplete image %u from %s. Dropping.\n", ib.img_id, macToHex(senderMac).c_str());
        ib.reset();
        return;
    }

    size_t totalSize = 0;
    for (uint16_t i = 0; i < ib.total_chunks; ++i) totalSize += ib.chunks[i].len;

    String header = "<IMG_START:" + macToHex(senderMac) + ":" + String(ib.img_id) + ":" + String((unsigned long)totalSize) + ">\n";
    Serial.print(header);

    for (uint16_t i = 0; i < ib.total_chunks; ++i) {
        if (ib.chunks[i].len > 0 && ib.chunks[i].data) {
            Serial.write(ib.chunks[i].data, ib.chunks[i].len);
        }
    }

    Serial.print("\n<IMG_END>\n");
    Serial.printf("Image %u from %s forwarded (size=%u bytes)\n", ib.img_id, macToHex(senderMac).c_str(), (unsigned)totalSize);
    ib.reset();
}

// ---------- ESP-NOW callback ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t *mac = info->src_addr;
    unsigned long now = millis();
    if (len <= 0) return;

    // CASE A: DONE packet
    if (len >= 5 && data[0] == 'D') {
        uint16_t img_id = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
        uint16_t total_chunks = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
        ImageBuffer *ib = nullptr;
        if (compareMAC(mac, sender1MAC)) ib = &buf1;
        else if (compareMAC(mac, sender2MAC)) ib = &buf2;
        if (ib && ib->img_id == img_id) {
            ib->total_chunks = total_chunks;
            tryFinalizeImage(*ib, mac);
        }
        return;
    }

    // CASE B: Image chunk packet
    if (len >= 7 && data[0] == 'I') {
        uint16_t img_id = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
        uint16_t seq = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
        uint16_t total = (uint16_t)data[5] | ((uint16_t)data[6] << 8);
        const uint8_t *payload = data + 7;
        int payloadLen = len - 7;

        ImageBuffer *ib = nullptr;
        if (compareMAC(mac, sender1MAC)) ib = &buf1;
        else if (compareMAC(mac, sender2MAC)) ib = &buf2;
        else return;

        if (ib->img_id != img_id) {
            if (ib->img_id != 0 && (now - ib->lastUpdate) < IMAGE_TIMEOUT_MS) {
                Serial.printf("New img_id %u from %s while previous %u incomplete. Resetting.\n",
                              img_id, macToHex(mac).c_str(), ib->img_id);
            }
            if (!ib->allocChunks(total == 0 ? 200 : total)) {
                Serial.println("Failed to alloc image chunks. Dropping.");
                return;
            }
            ib->img_id = img_id;
            ib->lastUpdate = now;
        }

        if (seq >= ib->total_chunks) return;
        if (ib->chunks[seq].data != nullptr) {
            ib->lastUpdate = now;
            return;
        }

        ib->chunks[seq].data = (uint8_t*)malloc(payloadLen);
        if (!ib->chunks[seq].data) {
            Serial.println("OOM allocating chunk. Dropping buffer.");
            ib->reset();
            return;
        }
        memcpy(ib->chunks[seq].data, payload, payloadLen);
        ib->chunks[seq].len = payloadLen;
        ib->received_chunks++;
        ib->lastUpdate = now;

        if (ib->received_chunks == ib->total_chunks) tryFinalizeImage(*ib, mac);
        return;
    }

    // CASE C: Sensor packets
    if (compareMAC(mac, sender1MAC) && len == sizeof(SensorPacket1)) memcpy(&packet1, data, sizeof(SensorPacket1));
    else if (compareMAC(mac, sender2MAC) && len == sizeof(SensorPacket2)) memcpy(&packet2, data, sizeof(SensorPacket2));
}
