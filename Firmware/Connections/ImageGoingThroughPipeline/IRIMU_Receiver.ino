#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ---------- Structs from Senders (UNMODIFIED) ----------
typedef struct {
  bool sensor1;
  bool sensor2;
  bool sensor3;
  bool sensor4;
  float accelX;
  float accelY;
  float accelZ;
  float gyroX;
  float gyroY;
  float gyroZ;
} SensorPacket1;

typedef struct {
  bool sensor5;
  bool sensor6;
  bool sensor7;
  bool sensor8;
} SensorPacket2;

// ---------- MAC Addresses for Each Sender (keep these correct) ----------
uint8_t sender1MAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C}; // Green Breadboard
uint8_t sender2MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64}; // Black Breadboard

// ---------- Global Sensor Storage (UNCHANGED) ----------
SensorPacket1 packet1;
SensorPacket2 packet2;

// ---------- Image buffering structures ----------
#define MAX_CHUNK_PAYLOAD  (200 - 7) // 7-byte header used by sender
#define IMAGE_TIMEOUT_MS   5000      // drop incomplete images after 5s of inactivity

struct ChunkEntry {
  uint8_t *data;
  size_t len;
  ChunkEntry() : data(nullptr), len(0) {}
};

struct ImageBuffer {
  uint16_t img_id = 0;
  uint16_t total_chunks = 0;
  uint16_t received_chunks = 0;
  ChunkEntry *chunks = nullptr;   // array length == total_chunks
  unsigned long lastUpdate = 0;

  void reset() {
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
};

// one buffer per sender
ImageBuffer buf1;
ImageBuffer buf2;

// ---------- Helper functions ----------
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
  for (int i = 0; i < 6; i++) {
    if (mac1[i] != mac2[i]) return false;
  }
  return true;
}

String macToHex(const uint8_t *mac) {
  char tmp[13];
  sprintf(tmp, "%02X%02X%02X%02X%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(tmp);
}

// allocate ImageBuffer->chunks for total_chunks
bool allocChunks(ImageBuffer &ib, uint16_t total_chunks) {
  ib.reset();
  ib.chunks = (ChunkEntry*)calloc(total_chunks, sizeof(ChunkEntry));
  if (!ib.chunks) return false;
  ib.total_chunks = total_chunks;
  ib.received_chunks = 0;
  ib.lastUpdate = millis();
  return true;
}

// try to concatenate and send image over Serial with framing markers
void tryFinalizeImage(ImageBuffer &ib, const uint8_t *senderMac) {
  if (ib.img_id == 0) return;
  if (ib.total_chunks == 0) return;

  if (ib.received_chunks != ib.total_chunks) {
    // incomplete - drop and reset
    // Serial.printf("Incomplete image %u from %s (have %u / %u). Dropping.\n",
   //               ib.img_id, macToHex(senderMac).c_str(), ib.received_chunks, ib.total_chunks);
    ib.reset();
    return;
  }

  // compute total size
  size_t totalSize = 0;
  for (uint16_t i = 0; i < ib.total_chunks; ++i) totalSize += ib.chunks[i].len;

  // Send header line so Python can parse and read exact bytes
  // Format: <IMG_START:MACHEX:IMGID:SIZE>\n
  String header = "<IMG_START:";
  header += macToHex(senderMac);
  header += ":";
  header += String(ib.img_id);
  header += ":";
  header += String((unsigned long)totalSize);
  header += ">\n";
  // Serial.print(header);

  // Write binary image
  for (uint16_t i = 0; i < ib.total_chunks; ++i) {
    if (ib.chunks[i].len > 0 && ib.chunks[i].data) {
      // Serial.write(ib.chunks[i].data, ib.chunks[i].len);
    }
  }

  // Send end marker
  // Serial.print("\n<IMG_END>\n");
  // Serial.printf("Image %u from %s forwarded (size=%u bytes)\n", ib.img_id, macToHex(senderMac).c_str(), (unsigned)totalSize);

  // cleanup
  ib.reset();
}

// ---------- ESP-NOW Callback ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
  unsigned long now = millis();

  // Quick safety check
  if (len <= 0) return;

  // CASE A: DONE packet from our modified senders:
  // Done packet format from senders: 5 bytes: ['D', img_id_lo, img_id_hi, total_lo, total_hi]
  if (len >= 1 && data[0] == 'D' && len >= 5) {
    uint16_t img_id = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    uint16_t total_chunks = (uint16_t)data[3] | ((uint16_t)data[4] << 8);

    ImageBuffer *ib = nullptr;
    if (compareMAC(mac, sender1MAC)) ib = &buf1;
    else if (compareMAC(mac, sender2MAC)) ib = &buf2;

    if (ib) {
      // if img_id matches current buffer -> finalize
      if (ib->img_id == img_id) {
        ib->total_chunks = total_chunks; // ensure total is updated (should match)
        tryFinalizeImage(*ib, mac);
      } else {
        // DONE for unknown image - ignore
        // Serial.printf("Received DONE for unknown img %u from %s\n", img_id, macToHex(mac).c_str());
      }
    }
    return;
  }

  // CASE B: Image chunk packets with 7-byte header:
  // ['I', img_id_lo, img_id_hi, seq_lo, seq_hi, total_lo, total_hi] + payload
  if (len >= 7 && data[0] == 'I') {
    uint16_t img_id   = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    uint16_t seq      = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    uint16_t total    = (uint16_t)data[5] | ((uint16_t)data[6] << 8);
    const uint8_t *payload = data + 7;
    int payloadLen = len - 7;

    ImageBuffer *ib = nullptr;
    if (compareMAC(mac, sender1MAC)) ib = &buf1;
    else if (compareMAC(mac, sender2MAC)) ib = &buf2;
    else {
      // unknown sender: ignore
      return;
    }

    // if new image id, allocate buffer
    if (ib->img_id != img_id) {
      // if previous buffer exists but incomplete and timed out, reset it
      if (ib->img_id != 0) {
        unsigned long age = now - ib->lastUpdate;
        if (age < IMAGE_TIMEOUT_MS) {
          // previously active image not finished but different id -> free old buffer
          // Serial.printf("New img_id %u arrived from %s while previous %u incomplete. Resetting old buffer.\n",
                //        img_id, macToHex(mac).c_str(), ib->img_id);
        }
        ib->reset();
      }
      // allocate for the incoming total (if total==0, be conservative)
      if (total == 0) total = 200; // fallback large number (shouldn't happen)
      if (!allocChunks(*ib, total)) {
        // Serial.println("Failed to alloc image chunks (OOM). Dropping image.");
        return;
      }
      ib->img_id = img_id;
      ib->total_chunks = total;
      ib->lastUpdate = now;
    }

    // bounds check
    if (seq >= ib->total_chunks) {
      // Serial.printf("Received seq %u >= total %u from %s (img %u). Ignoring.\n", seq, ib->total_chunks, macToHex(mac).c_str(), img_id);
      return;
    }

    // if we already have this chunk, ignore duplicate
    if (ib->chunks[seq].data != nullptr) {
      // duplicate chunk
      ib->lastUpdate = now;
      return;
    }

    // store chunk
    ib->chunks[seq].data = (uint8_t*)malloc(payloadLen);
    if (!ib->chunks[seq].data) {
      // Serial.println("OOM allocating chunk; dropping image buffer.");
      ib->reset();
      return;
    }
    memcpy(ib->chunks[seq].data, payload, payloadLen);
    ib->chunks[seq].len = payloadLen;
    ib->received_chunks++;
    ib->lastUpdate = now;

    // If we've already received all chunks, we can finalize even before DONE arrives
    if (ib->received_chunks == ib->total_chunks) {
      tryFinalizeImage(*ib, mac);
    }

    return;
  }

  // CASE C: Sensor packets (unchanged logic)
  // Sender 1 sensor struct
  if (compareMAC(mac, sender1MAC) && len == sizeof(SensorPacket1)) {
    memcpy(&packet1, data, sizeof(SensorPacket1));
    return;
  }
  // Sender 2 sensor struct
  if (compareMAC(mac, sender2MAC) && len == sizeof(SensorPacket2)) {
    memcpy(&packet2, data, sizeof(SensorPacket2));
    return;
  }

  // otherwise ignore (image bytes that don't match header are ignored)
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    // Serial.println("ESP-NOW init failed!");
    while (1);
  }

  esp_now_register_recv_cb(OnDataRecv);

  // Serial.println("Receiver initialized and ready for multiple senders (images + sensors).");
}

// ---------- Loop ----------
void loop() {
  static unsigned long lastPlotTime = 0;
  static unsigned long lastTerminalTime = 0;
  unsigned long now = millis();
  bool plotIR = false;

  // drop stale image buffers
  if (now % 1000 < 50) { // roughly every second
    if (buf1.img_id != 0 && (now - buf1.lastUpdate) > IMAGE_TIMEOUT_MS) {
      // Serial.printf("Timeout: dropping incomplete image %u from %s\n", buf1.img_id, macToHex(sender1MAC).c_str());
      buf1.reset();
    }
    if (buf2.img_id != 0 && (now - buf2.lastUpdate) > IMAGE_TIMEOUT_MS) {
      // Serial.printf("Timeout: dropping incomplete image %u from %s\n", buf2.img_id, macToHex(sender2MAC).c_str());
      buf2.reset();
    }
  }

  // ---------- Serial Plotter for 8 IR sensors ----------
  if (now - lastPlotTime >= 100) { // plot every 100ms
    if (plotIR) {
      Serial.print("S1:"); Serial.print(packet1.sensor1);
      Serial.print(" S2:"); Serial.print(packet1.sensor2);
      Serial.print(" S3:"); Serial.print(packet1.sensor3);
      Serial.print(" S4:"); Serial.print(packet1.sensor4);
      Serial.print(" S5:"); Serial.print(packet2.sensor5);
      Serial.print(" S6:"); Serial.print(packet2.sensor6);
      Serial.print(" S7:"); Serial.print(packet2.sensor7);
      Serial.print(" S8:"); Serial.println(packet2.sensor8);
    } else {
      Serial.print("AccX:"); Serial.print(packet1.accelX);
      Serial.print(" AccY:"); Serial.print(packet1.accelY);
      Serial.print(" AccZ:"); Serial.print(packet1.accelZ);
      Serial.print(" GryX:"); Serial.print(packet1.gyroX);
      Serial.print(" GryY:"); Serial.print(packet1.gyroY);
      Serial.print(" GryZ:"); Serial.println(packet1.gyroZ);
    }
    lastPlotTime = now;
  }

  // ---------- Serial Monitor for simple debug ----------
  if (now - lastTerminalTime >= 1000) {
    lastTerminalTime = now;
  }
}
