#include <esp_now.h>
#include <WiFi.h>

// Struct must match sender
typedef struct {
  uint16_t distances[64];
  bool sensorOne;
  bool sensorTwo;
  bool sensorThree;
  bool sensorFour;
} SensorData;

SensorData receivedData;

// NEW callback style for ESP-IDF v5
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  Serial.println("Received 8x8 Distance Data (mm):");
  for (int i = 0; i < 64; i++) {
    // For pyserial, the structure is going to be:
    // 0, 1, 2, 3, 4, 5, ...
    // ...
    // ...
    // 0, 1, 2, 3, 4, 5, ...
    Serial.print(receivedData.distances[i]);
    if ((i + 1) % 8 == 0) Serial.println();
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  // ESP-NOW setup
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callback
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
}
