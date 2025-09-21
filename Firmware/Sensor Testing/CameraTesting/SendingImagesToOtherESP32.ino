// This code captures images using the ArduCAM Mini 2MP Plus OV2640
// and sends them to another ESP32 using ESP-NOW protocol.

// THIS HAS NOT BEEN TESTED YET.

#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"
#include <WiFi.h>
#include <esp_now.h>

// Make sure memorysaver.h has this uncommented:
// #define OV2640_MINI_2MP

#define CS 5  // Chip select pin for ArduCAM
#define CAMERA_INTERVAL 15000 // 15 seconds

ArduCAM myCAM(OV2640, CS);

// Receiver MAC address (replace with your receiver ESP32 MAC)
uint8_t receiverAddress[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};

// Maximum ESP-NOW payload per packet
#define CHUNK_SIZE 250

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // SPI pins for ESP32
  SPI.begin(18, 19, 23, CS); // SCK, MISO, MOSI, CS
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  // Initialize camera
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);

  uint8_t temp;
  while (1) {
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) { delay(1000); continue; }
    break;
  }

  uint8_t vid, pid;
  while (1) {
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid == 0x26) && (pid == 0x41 || pid == 0x42)) break;
    delay(1000);
  }

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  delay(100);
  myCAM.clear_fifo_flag();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void sendChunk(uint8_t *data, size_t len) {
  esp_err_t result = esp_now_send(receiverAddress, data, len);
  if (result != ESP_OK) {
    Serial.println("Error sending chunk");
  }
}

void loop() {
  // Capture image
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) delay(10);

  uint32_t length = myCAM.read_fifo_length();
  Serial.print("IMG_LEN: "); Serial.println(length);

  // Read image into buffer
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  uint8_t temp;
  uint32_t count = 0;
  uint8_t chunk[CHUNK_SIZE];

  while (length--) {
    temp = SPI.transfer(0x00);
    chunk[count++] = temp;
    if (count == CHUNK_SIZE || length == 0) {
      sendChunk(chunk, count);
      count = 0;
    }
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();
  Serial.println("IMG_DONE");

  delay(CAMERA_INTERVAL);
}
