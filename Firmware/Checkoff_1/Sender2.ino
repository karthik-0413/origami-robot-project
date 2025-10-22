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
// header: 1 byte type + 2 bytes img_id + 2 bytes seq + 2 bytes total = 7 bytes
#define IMG_HEADER_SIZE 7
#define IMG_PAYLOAD_MAX (CHUNK_SIZE - IMG_HEADER_SIZE)

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
// IMAGE TRANSMIT HELPERS (only these parts changed)
// ==========================================================
static uint16_t g_image_counter = 0;

bool sendImagePacketWithHeader(uint8_t *payload, size_t payloadLen, uint16_t img_id, uint16_t seq, uint16_t total) {
  // Build buffer: [type(1)] [img_id(2)] [seq(2)] [total(2)] [payload]
  uint8_t packet[CHUNK_SIZE];
  packet[0] = 'I'; // Image chunk
  packet[1] = img_id & 0xFF;
  packet[2] = (img_id >> 8) & 0xFF;
  packet[3] = seq & 0xFF;
  packet[4] = (seq >> 8) & 0xFF;
  packet[5] = total & 0xFF;
  packet[6] = (total >> 8) & 0xFF;

  memcpy(&packet[IMG_HEADER_SIZE], payload, payloadLen);
  size_t sendLen = IMG_HEADER_SIZE + payloadLen;

  for (int attempt = 0; attempt < 2; ++attempt) {
    esp_err_t res = esp_now_send(receiverMAC, packet, sendLen);
    if (res == ESP_OK) return true;
    delay(4);
  }
  return false;
}

void sendImageDone(uint16_t img_id, uint16_t total) {
  uint8_t buf[5];
  buf[0] = 'D'; // Done marker
  buf[1] = img_id & 0xFF;
  buf[2] = (img_id >> 8) & 0xFF;
  buf[3] = total & 0xFF;
  buf[4] = (total >> 8) & 0xFF;
  esp_now_send(receiverMAC, buf, sizeof(buf));
}

// Capture and send image in ordered chunks with header
void captureAndSend() {
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

  uint32_t length = myCAM.read_fifo_length();
  if (length == 0) {
    myCAM.clear_fifo_flag();
    return;
  }

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  uint8_t chunkBuf[IMG_PAYLOAD_MAX];
  uint32_t sent = 0;
  uint16_t img_id = ++g_image_counter;
  uint16_t total_chunks = (length + IMG_PAYLOAD_MAX - 1) / IMG_PAYLOAD_MAX;
  uint16_t seq = 0;

  while (sent < length) {
    size_t toRead = min((size_t)IMG_PAYLOAD_MAX, (size_t)(length - sent));
    for (size_t i = 0; i < toRead; i++) chunkBuf[i] = SPI.transfer(0x00);

    // send with header (type 'I')
    sendImagePacketWithHeader(chunkBuf, toRead, img_id, seq, total_chunks);

    sent += toRead;
    seq++;
    delay(5); // same pacing
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  // final DONE message with image id & total_chunks
  sendImageDone(img_id, total_chunks);
}

// ==========================================================
// SENSOR FUNCTIONS (UNCHANGED)
// ==========================================================
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
}

void sendSensorData() {
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&sensorData, sizeof(sensorData));
  (void)result;
}

// ==========================================================
// ARDUINO MAIN (UNCHANGED)
// ==========================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  initCamera();
  initVL53L0X();
  initESPNow();
}

void loop() {
  captureAndSend();   // Send camera image (now with headers)
  readVL53L0X();      // Update bools in struct
  sendSensorData();   // Send full struct (bools)
}
