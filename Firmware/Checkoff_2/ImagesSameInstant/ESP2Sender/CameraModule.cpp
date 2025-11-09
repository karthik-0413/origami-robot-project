#include "CameraModule.h"
#include <SPI.h>
#include "Comms.h"

ArduCAM myCAM(OV2640, CS);

/****************************************************
 * Function: initCamera
 * Description: Initializes the ArduCAM OV2640 Mini Module
 *              with optimized settings for speed.
 * Inputs: None
 * Outputs: None
 * Notes: Uses SPI & I2C Pins on ESP32
 ****************************************************/
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
  myCAM.OV2640_set_JPEG_size(OV2640_640x480);
  myCAM.clear_fifo_flag();
}

/****************************************************
 * Function: captureAndSend
 * Description: Captures image and sends it via UDP
 *              using optimized bulk SPI read.
 * Inputs: None
 * Outputs: None
 * Notes: 
 *    - Uses malloc for dynamic buffer allocation
 *    - Sends image in UDP chunks
 *    - Includes timing diagnostics
 ****************************************************/
void captureAndSend() {
  unsigned long t1 = millis();
  
  myCAM.flush_fifo();                        // Clear any previous data in FIFO
  myCAM.clear_fifo_flag();                   // Reset capture done flag
  myCAM.start_capture();                     // Start capturing image

  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)); // Wait until capture is complete
  
  unsigned long t2 = millis();

  uint32_t length = myCAM.read_fifo_length();  // Get the total number of bytes captured
  if (length == 0) {                           // If no data, reset FIFO and exit
    myCAM.clear_fifo_flag();                          
    return;                                          
  }

  // Allocate buffer for entire image
  uint8_t* imageBuffer = (uint8_t*)malloc(length);
  if (!imageBuffer) {
    Serial.println("Failed to allocate image buffer");
    myCAM.clear_fifo_flag();
    return;
  }

  myCAM.CS_LOW();                              // Select camera for SPI burst read
  myCAM.set_fifo_burst();                      // Enable burst mode for faster SPI read

  // Fast bulk read using SPI transfer
  SPI.transferBytes(NULL, imageBuffer, length);

  myCAM.CS_HIGH();                             // Deselect camera after SPI read
  myCAM.clear_fifo_flag();                     // Reset FIFO flags

  unsigned long t3 = millis();
  
  // Send via UDP (non-blocking, consistent timing)
  bool success = sendImageUDP(imageBuffer, length);
  
  unsigned long t4 = millis();
  
  Serial.printf("Capture: %lums, Read: %lums, Send: %lums, Total: %lums\n", 
                t2-t1, t3-t2, t4-t3, t4-t1);
  
  free(imageBuffer);                           // Free allocated memory
  
  if (!success) {
    Serial.println("Image send failed");
  }
}