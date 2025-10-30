#include "CameraModule.h"
#include <SPI.h>
#include <esp_now.h>
#include "Comms.h"

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

    sendImagePacketWithHeader(chunkBuf, toRead, img_id, seq, total_chunks);
    sent += toRead;
    seq++;
    delay(5);
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();
  sendImageDone(img_id, total_chunks);
}
