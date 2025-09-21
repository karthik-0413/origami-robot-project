#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <SparkFun_VL53L5CX_Library.h>

// IR Sensor Pin
int IRSensorSide1 = 2;
int IRSensorSide2 = 3;
int IRSensorSide3 = 4;
int IRSensorSide4 = 5;

// Create sensor object
SparkFun_VL53L5CX myImager;

// Define struct to hold 64 distance values
typedef struct {
  uint16_t distances[64];  // 64 zones (8x8)
  bool sensorOne;
  bool sensorTwo;
  bool sensorThree;
  bool sensorFour;
} SensorData;

SensorData sendData;

uint8_t receiverMAC[] = {0x14, 0x33, 0x5c, 0x02, 0x88, 0x34};

void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Setup Corresponding I/O Pins
  pinMode(IRSensorSide1, INPUT);
  pinMode(IRSensorSide2, INPUT);
  pinMode(IRSensorSide3, INPUT);
  pinMode(IRSensorSide4, INPUT);

  // Setup ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  // Setup Receiving Data
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Setup ToF Camera
  if (myImager.begin() == false) {
    Serial.println("VL53L5CX not found");
    while (1);
  }

  myImager.setResolution(8 * 8);
  myImager.setRangingFrequency(15);
  myImager.startRanging();
}

void loop() {
  if (myImager.isDataReady()) {
    VL53L5CX_ResultsData measurementData;
    if (myImager.getRangingData(&measurementData)) {
      
      sendData.sensorOne = digitalRead(IRSensorSide1);
      sendData.sensorTwo = digitalRead(IRSensorSide2);
      sendData.sensorThree = digitalRead(IRSensorSide3);
      sendData.sensorFour = digitalRead(IRSensorSide4);
        // LOW = Detected
        // HIGH = Not Detected
      
      for (int i = 0; i < 64; i++) {
        sendData.distances[i] = measurementData.distance_mm[i];
      }

      // Send struct via ESP-NOW
      esp_err_t result = esp_now_send(receiverMAC, (uint8_t *) &sendData, sizeof(sendData));

      if (result == ESP_OK) {
        Serial.println("Sent data successfully");
      } else {
        Serial.println("Error sending data");
      }
    }
  }
}
