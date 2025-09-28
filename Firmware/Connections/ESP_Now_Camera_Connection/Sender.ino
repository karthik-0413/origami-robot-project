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

// ---------- VL53L0X ----------
Adafruit_VL53L0X lox1, lox2, lox3, lox4;
#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

// ---------- IMU ----------
Adafruit_MPU6050 mpu;
unsigned long lastMPUTime = 0;

// ---------- ArduCAM ----------
#define CS 5
ArduCAM myCAM(OV2640, CS);
#define CHUNK_SIZE 200
uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C}; // receiver MAC

// ---------- Functions ----------
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Optional: remove for speed
}

void initVL53L0XContinuous(Adafruit_VL53L0X &lox, int xshutPin, int address) {
  pinMode(xshutPin, OUTPUT);
  digitalWrite(xshutPin, HIGH);
  delay(5);
  lox.begin(address);
  lox.startRangeContinuous();
}

void readVL53L0XContinuous(const char *label, Adafruit_VL53L0X &lox) {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);  // this works continuously if called repeatedly
  if (measure.RangeStatus != 4) {
    // Optional: remove Serial for speed
  }
}

void captureAndSend() {
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  // wait for capture
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
    // Removed delays for speed
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin(18, 19, 23, CS);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  // --------- Camera setup ----------
  myCAM.write_reg(0x07, 0x80);
  delay(10);
  myCAM.write_reg(0x07, 0x00);
  delay(10);
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  myCAM.clear_fifo_flag();

  // --------- VL53L0X continuous mode ----------
  initVL53L0XContinuous(lox1, XSHUT1, LOX1_ADDRESS);
  initVL53L0XContinuous(lox2, XSHUT2, LOX2_ADDRESS);
  initVL53L0XContinuous(lox3, XSHUT3, LOX3_ADDRESS);
  initVL53L0XContinuous(lox4, XSHUT4, LOX4_ADDRESS);

  // --------- MPU setup ----------
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // --------- ESP-NOW setup ----------
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) while (1);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) while (1);
}

void loop() {
  // ---- VL53L0X readings ----
  readVL53L0XContinuous("S1", lox1);
  readVL53L0XContinuous("S2", lox2);
  readVL53L0XContinuous("S3", lox3);
  readVL53L0XContinuous("S4", lox4);

  // ---- MPU readings every 50ms ----
  if (millis() - lastMPUTime >= 50) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    lastMPUTime = millis();
  }

  // ---- Capture & send camera image ----
  captureAndSend();
}
