// Black Breadboard

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduCAM.h>
#include "memorysaver.h"

// ---------- Config ----------
#define CHUNK_SIZE 200
uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34}; // replace with your receiver MAC

// Camera
#define CS 5
ArduCAM myCAM(OV2640, CS);

// VL53L0X Sensors
Adafruit_VL53L0X lox1, lox2, lox3, lox4;
#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

// Struct for sending sensor + IMU data
typedef struct {
  bool sensor5;
  bool sensor6;
  bool sensor7;
  bool sensor8;
} SensorPacket;

SensorPacket sensorData;

// ---------- ESP-NOW ----------
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

// ==========================================================
// INITIALIZATION FUNCTIONS
// ==========================================================
void initCamera() {
  SPI.begin(18, 19, 23, CS);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  myCAM.write_reg(0x07, 0x80);
  delay(10);
  myCAM.write_reg(0x07, 0x00);
  delay(10);
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  myCAM.clear_fifo_flag();
  Serial.println("Camera initialized.");
}

void initVL53L0X() {
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);

  // Reset all
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
  delay(10);

  // Power on one by one + assign address
  digitalWrite(XSHUT1, HIGH); delay(10);
  lox1.begin(LOX1_ADDRESS);

  digitalWrite(XSHUT2, HIGH); delay(10);
  lox2.begin(LOX2_ADDRESS);

  digitalWrite(XSHUT3, HIGH); delay(10);
  lox3.begin(LOX3_ADDRESS);

  digitalWrite(XSHUT4, HIGH); delay(10);
  lox4.begin(LOX4_ADDRESS);

  Serial.println("VL53L0X sensors initialized.");
}

void initESPNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while (1);
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer.");
    while (1);
  }
  Serial.println("ESP-NOW initialized.");
}

// ==========================================================
// SENSOR FUNCTIONS
// ==========================================================
void captureAndSend() {
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

  uint32_t length = myCAM.read_fifo_length();
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  uint8_t buf[CHUNK_SIZE];
  uint32_t sent = 0;

  while (sent < length) {
    size_t toSend = min((size_t)CHUNK_SIZE, (size_t)(length - sent));
    for (size_t i = 0; i < toSend; i++) buf[i] = SPI.transfer(0x00);
    esp_now_send(receiverMAC, buf, toSend);
    sent += toSend;
    delay(5);
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  const char doneMarker[] = "IMG_DONE";
  esp_now_send(receiverMAC, (uint8_t*)doneMarker, 8);
}

void readVL53L0X() {
  VL53L0X_RangingMeasurementData_t measure[4];
  lox1.rangingTest(&measure[0], false);
  lox2.rangingTest(&measure[1], false);
  lox3.rangingTest(&measure[2], false);
  lox4.rangingTest(&measure[3], false);

  bool results[4] = {false, false, false, false};

  for (int i = 0; i < 4; i++) {
    if (measure[i].RangeStatus != 4) {
      if (measure[i].RangeMilliMeter <= 50) {
        results[i] = true; // DETECT
      } else {
        results[i] = false; // CLEAR
      }
    }
  }

  // Save into struct
  sensorData.sensor5 = results[0];
  sensorData.sensor6 = results[1];
  sensorData.sensor7 = results[2];
  sensorData.sensor8 = results[3];

  // Serial.printf("S1:%d S2:%d S3:%d S4:%d\n", 
  //               sensorData.sensor1, sensorData.sensor2, 
  //               sensorData.sensor3, sensorData.sensor4);
}

void sendSensorData() {
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&sensorData, sizeof(sensorData));
  // if (result == ESP_OK) {
  //   Serial.println("Sensor data sent successfully");
  // } else {
  //   Serial.println("Error sending sensor+IMU data");
  // }
}

// ==========================================================
// ARDUINO MAIN
// ==========================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  initCamera();
  initVL53L0X();
  initESPNow();
}

void loop() {
  captureAndSend();   // Send camera image
  readVL53L0X();      // Update bools in struct
  sendSensorData();   // Send full struct (bools + IMU)
}
