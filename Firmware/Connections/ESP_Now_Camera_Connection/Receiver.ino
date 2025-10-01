#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define MAX_IMAGE_SIZE 65536
uint8_t imageBuffer[MAX_IMAGE_SIZE];
uint32_t imageLength = 0;
uint32_t imageCount = 0;
unsigned long lastTime = 0;

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == 8 && memcmp(data, "IMG_DONE", 8) == 0) {
    imageCount++;
    imageLength = 0;
    return;
  }

  if (imageLength + len < MAX_IMAGE_SIZE) {
    memcpy(imageBuffer + imageLength, data, len);
    imageLength += len;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) while(1);
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
