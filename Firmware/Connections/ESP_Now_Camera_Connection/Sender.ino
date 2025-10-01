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

#define CHUNK_SIZE 200
uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34}; // replace with your receiver MAC

// Camera
#define CS 5
ArduCAM myCAM(OV2640, CS);

// VL53L0X
Adafruit_VL53L0X lox1, lox2, lox3, lox4;
#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

// MPU
Adafruit_MPU6050 mpu;
unsigned long lastMPUTime = 0;

// Callback for sent ESP-NOW packets (optional)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

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

    // Send the chunk and wait a tiny bit to improve reliability
    esp_now_send(receiverMAC, buf, toSend);
    sent += toSend;
    delay(5); 
  }
  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  // Send "IMG_DONE" marker so receiver knows image is complete
  const char doneMarker[] = "IMG_DONE";
  esp_now_send(receiverMAC, (uint8_t*)doneMarker, 8);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin(18, 19, 23, CS);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  // Camera init
  myCAM.write_reg(0x07, 0x80);
  delay(10);
  myCAM.write_reg(0x07, 0x00);
  delay(10);
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  myCAM.clear_fifo_flag();

  // MPU init
  if (!mpu.begin()) while(1);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) while(1);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) while(1);
}

void loop() {
  // Read MPU every 50ms (optional, you can also send via ESP-NOW)
  if (millis() - lastMPUTime >= 50) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    lastMPUTime = millis();
  }

  // Capture and send camera image
  captureAndSend();
}
