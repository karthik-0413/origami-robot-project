// This is a simple example to capture images using the ArduCAM Mini 2MP Plus OV2640
// I tested this by connecting the ESP32-CAM module to my computer and using the Serial Monitor
// to view the output. The captured images are sent over Serial in JPEG format.

#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

// Make sure memorysaver.h has this uncommented:
// #define OV2640_MINI_2MP

#define CS 5  // Chip select pin for ArduCAM

ArduCAM myCAM(OV2640, CS);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // SPI pins for ESP32
  SPI.begin(18, 19, 23, CS); // SCK, MISO, MOSI, CS
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  // Reset ArduCAM CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);

  // Test SPI interface
  uint8_t temp;
  while (1) {
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) {
      Serial.println("SPI interface Error! Retrying...");
      delay(1000);
      continue;
    }
    Serial.println("SPI interface OK");
    break;
  }

  // Detect OV2640 camera
  uint8_t vid, pid;
  while (1) {
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    if ((vid != 0x26) || ((pid != 0x41) && (pid != 0x42))) {
      Serial.println("Cannot find OV2640 module! Retrying...");
      delay(1000);
      continue;
    }
    Serial.println("OV2640 detected!");
    break;
  }

  // Initialize camera in JPEG mode
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  delay(100);
  myCAM.clear_fifo_flag();
}

void loop() {
  // Capture image
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  // Wait for capture to finish
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    delay(10);
  }

  // Read image length
  uint32_t length = myCAM.read_fifo_length();
  Serial.print("IMG_LEN:");
  Serial.println(length);

  // Send image over Serial
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  uint8_t temp, temp_last = 0;
  while (length--) {
    temp_last = temp;
    temp = SPI.transfer(0x00);
    Serial.write(temp);
  }
  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  Serial.println("IMG_DONE");

  // Wait 15 seconds before next capture
  delay(1000);
}
