#include "CameraModule.h"
#include <SPI.h>

ArduCAM myCAM(OV2640, CS);
static uint16_t g_image_counter = 0;

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
}

void captureAndSend(WiFiClient* client) {
  if (!client || !client->connected()) return; // Skip if TCP not connected

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

  // Allocate a single buffer for the full image
  uint8_t* imageBuf = (uint8_t*)malloc(length);
  if (!imageBuf) {
      Serial.println("OOM allocating image buffer");
      myCAM.CS_HIGH();
      myCAM.clear_fifo_flag();
      return;
  }

  // Read the entire image into the buffer
  for (uint32_t i = 0; i < length; i++) {
      imageBuf[i] = SPI.transfer(0x00);
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  // ---- Send length first ----
  uint8_t lenBuf[4];
  lenBuf[0] = (length >> 0) & 0xFF;
  lenBuf[1] = (length >> 8) & 0xFF;
  lenBuf[2] = (length >> 16) & 0xFF;
  lenBuf[3] = (length >> 24) & 0xFF;
  client->write(lenBuf, 4);  // Send 4-byte length header

  // Send over TCP directly
  client->write(imageBuf, length);

  free(imageBuf); // Free memory
}

