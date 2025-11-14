#include "CameraModule.h"
#include <SPI.h>
#include "Comms.h"

ArduCAM myCAM(OV2640, CS);

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

    // Grayscale for smaller file size
    myCAM.OV2640_set_Special_effects(BW);

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

    uint8_t* buffer = (uint8_t*)malloc(length);
    if (!buffer) { myCAM.clear_fifo_flag(); return; }

    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    SPI.transferBytes(NULL, buffer, length);
    myCAM.CS_HIGH();
    myCAM.clear_fifo_flag();

    bool ok = sendImageUDP(buffer, length);
    if (!ok) Serial.println("Image send failed");
    free(buffer);
}
