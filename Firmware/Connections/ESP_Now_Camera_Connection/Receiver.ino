#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define CHUNK_SIZE 250  // Must match sender
#define MAX_IMAGE_SIZE 65536  // Max buffer size (64 KB)

uint8_t imageBuffer[MAX_IMAGE_SIZE];
uint32_t imageLength = 0;      // Total bytes received for current image
uint32_t imageCount = 0;       // Total images received
unsigned long lastTime = 0;    // Timer for 1-second FPS counting

uint8_t senderMAC[6]; // optional: store sender MAC

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // Optional: copy sender MAC
  memcpy(senderMAC, info->src_addr, 6);

  // Append received chunk to image buffer
  if ((imageLength + len) < MAX_IMAGE_SIZE) {
    memcpy(imageBuffer + imageLength, data, len);
    imageLength += len;
  }

  // Check for end-of-image marker (IMG_DONE sent from sender)
  // Here we detect "IMG_DONE" by assuming the last chunk sends a fixed marker
  // If you don’t have a marker, you can assume each image starts fresh after N bytes
  // For simplicity, assume imageLength resets every full image
  // Count a full image received
  if (len < CHUNK_SIZE) {  // Last chunk smaller than CHUNK_SIZE
    imageCount++;
    imageLength = 0;  // Reset for next image
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while (1);
  }

  esp_now_register_recv_cb(OnDataRecv);

  lastTime = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastTime >= 1000) {
    Serial.print("Images received in last second: ");
    Serial.println(imageCount);
    imageCount = 0;
    lastTime = now;
  }
}
