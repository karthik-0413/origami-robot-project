#include "Comms.h"
#include "CameraModule.h"

uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C};
WiFiUDP udpClient;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == 1 && data[0] == 0xC4) {
    Serial.println(">>> Trigger received - capturing <<<");
    captureAndSend();
    Serial.println(">>> Image sent <<<");
  }
}

void initWiFi() {
  WiFi.mode(WIFI_AP_STA);
  
  IPAddress local_IP(192, 168, 8, 100 + CAMERA_ID);
  IPAddress gateway(192, 168, 8, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 8, 1);
  
  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Receiver IP: %s\n", RECEIVER_IP);
  } else {
    Serial.println("\nWiFi FAILED!");
  }
}

void initESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED!");
    while (1);
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer!");
    while (1);
  }
  
  Serial.println("ESP-NOW ready - waiting for triggers");
}

bool sendImageUDP(uint8_t* imageData, uint32_t length) {
  // CRITICAL: Smaller chunks for reliability
  const size_t UDP_PAYLOAD = 512;  // Small chunks = reliable
  const size_t HEADER_SIZE = 5;
  
  uint16_t totalChunks = (length + UDP_PAYLOAD - 1) / UDP_PAYLOAD;
  
  Serial.printf("Sending %u bytes in %u chunks...\n", length, totalChunks);
  
  uint8_t *packet = (uint8_t*)malloc(HEADER_SIZE + UDP_PAYLOAD);
  if (!packet) {
    Serial.println("Malloc failed!");
    return false;
  }
  
  for (uint16_t seq = 0; seq < totalChunks; seq++) {
    size_t offset = seq * UDP_PAYLOAD;
    size_t chunkSize = min((size_t)UDP_PAYLOAD, (size_t)(length - offset));
    
    // Build packet header
    packet[0] = CAMERA_ID;
    packet[1] = (seq >> 8) & 0xFF;
    packet[2] = seq & 0xFF;
    packet[3] = (totalChunks >> 8) & 0xFF;
    packet[4] = totalChunks & 0xFF;
    
    // Copy chunk data
    memcpy(&packet[HEADER_SIZE], &imageData[offset], chunkSize);
    
    // Send with verification
    if (!udpClient.beginPacket(RECEIVER_IP, IMAGE_PORT)) {
      Serial.println("beginPacket failed!");
      free(packet);
      return false;
    }
    
    size_t written = udpClient.write(packet, HEADER_SIZE + chunkSize);
    if (written != HEADER_SIZE + chunkSize) {
      Serial.printf("Write failed: %d != %d\n", written, HEADER_SIZE + chunkSize);
    }
    
    if (!udpClient.endPacket()) {
      Serial.println("endPacket failed!");
    }
    
    // CRITICAL: Delay between chunks to prevent router overflow
    delay(3);  // 3ms between packets = reliable delivery
    
    // Print progress every 5 chunks
    if (seq % 5 == 0) {
      Serial.printf("  Progress: %u/%u chunks\n", seq + 1, totalChunks);
    }
    
    yield();
  }
  
  free(packet);
  Serial.println("All chunks sent!");
  return true;
}