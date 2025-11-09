#include "CameraModule.h"
#include <SPI.h>
#include "Comms.h"

ArduCAM myCAM(OV2640, CS);

void initCamera() {
  SPI.begin(18, 19, 23, CS);
  SPI.setFrequency(12000000);  // Moderate speed for reliability
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  myCAM.write_reg(0x07, 0x80);
  delay(10);
  myCAM.write_reg(0x07, 0x00);
  delay(10);

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  myCAM.write_reg(0x44, 0x28);  // Moderate quality for smaller files
  
  myCAM.OV2640_set_Light_Mode(Auto);
  myCAM.OV2640_set_Special_effects(Normal);
  
  myCAM.clear_fifo_flag();
  
  Serial.println("Camera ready!");
}

void captureAndSend() {
  Serial.println("Starting capture...");
  
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

  uint32_t length = myCAM.read_fifo_length();
  Serial.printf("Captured %u bytes\n", length);
  
  if (length == 0) {
    Serial.println("No data!");
    myCAM.clear_fifo_flag();
    return;
  }

  uint8_t* imageBuffer = (uint8_t*)malloc(length);
  if (!imageBuffer) {
    Serial.println("Malloc failed!");
    myCAM.clear_fifo_flag();
    return;
  }

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  SPI.transferBytes(NULL, imageBuffer, length);
  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();
  
  Serial.println("Image read from camera");

  sendImageUDP(imageBuffer, length);
  free(imageBuffer);
}