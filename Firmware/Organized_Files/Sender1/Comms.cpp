#include "Comms.h"

uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

void initESPNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) while (1);

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) while (1);
}

bool sendImagePacketWithHeader(uint8_t *payload, size_t payloadLen,
                               uint16_t img_id, uint16_t seq, uint16_t total) {
  const uint8_t IMG_HEADER_SIZE = 7;
  const size_t CHUNK_SIZE = 200;

  uint8_t packet[CHUNK_SIZE];
  packet[0] = 'I';
  packet[1] = img_id & 0xFF;
  packet[2] = (img_id >> 8) & 0xFF;
  packet[3] = seq & 0xFF;
  packet[4] = (seq >> 8) & 0xFF;
  packet[5] = total & 0xFF;
  packet[6] = (total >> 8) & 0xFF;

  memcpy(&packet[IMG_HEADER_SIZE], payload, payloadLen);
  size_t sendLen = IMG_HEADER_SIZE + payloadLen;

  for (int attempt = 0; attempt < 2; ++attempt) {
    if (esp_now_send(receiverMAC, packet, sendLen) == ESP_OK) return true;
    delay(4);
  }
  return false;
}

void sendImageDone(uint16_t img_id, uint16_t total) {
  uint8_t buf[5];
  buf[0] = 'D';
  buf[1] = img_id & 0xFF;
  buf[2] = (img_id >> 8) & 0xFF;
  buf[3] = total & 0xFF;
  buf[4] = (total >> 8) & 0xFF;
  esp_now_send(receiverMAC, buf, sizeof(buf));
}

void sendSensorData() {
  esp_now_send(receiverMAC, (uint8_t*)&sensorData, sizeof(sensorData));
}
