#include "CameraModule.h"
#include <SPI.h>
#include <esp_now.h>
#include "Comms.h"

ArduCAM myCAM(OV2640, CS);                  // Create camera object for OV2640 module, using CS pin
static uint16_t g_image_counter = 0;        // Global counter for image IDs

/****************************************************
 * Function: initCamera
 * Description: Initializes the ArduCAM OV2640 Mini Module
              with the specific settings.
 * Inputs: None
 * Outputs: None
 * Notes: Uses SPI & I2C Pins on ESP32
****************************************************/
void initCamera() {
  SPI.begin(18, 19, 23, CS);                  // Initialize SPI bus with SCK=18, MISO=19, MOSI=23, CS=5
  pinMode(CS, OUTPUT);                        // Set Chip Select pin as output
  digitalWrite(CS, HIGH);                     // Deselect camera by default

  myCAM.write_reg(0x07, 0x80);                // Reset the camera
  delay(10);                                  // Wait 10ms for reset
  myCAM.write_reg(0x07, 0x00);                // Clear the reset bit
  delay(10);                                  // Wait 10ms after clearing reset

  myCAM.set_format(JPEG);                     // Set image format to JPEG
  myCAM.InitCAM();                            // Initialize camera registers
  myCAM.OV2640_set_JPEG_size(OV2640_320x240); // Set capture resolution to 320x240
  myCAM.clear_fifo_flag();                    // Clear the FIFO buffer
}


/****************************************************
 * Function: captureAndSend
 * Description: Captures real-time image of surroundings
              and sends it via ESP NOW.
 * Inputs: None
 * Outputs: None
 * Notes: Uses Comm Class "sendImagePacketWithHeader" and
        "sendImageDone" function to send image.
****************************************************/
void captureAndSend() {
  myCAM.flush_fifo();                        // Clear any previous data in FIFO
  myCAM.clear_fifo_flag();                   // Reset capture done flag
  myCAM.start_capture();                     // Start capturing image

  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)); // Wait until capture is complete

  uint32_t length = myCAM.read_fifo_length();  // Get the total number of bytes captured
  if (length == 0) {                           // If no data, reset FIFO and exit
    myCAM.clear_fifo_flag();                          
    return;                                          
  }

  myCAM.CS_LOW();                              // Select camera for SPI burst read
  myCAM.set_fifo_burst();                      // Enable burst mode for faster SPI read

  uint8_t  chunkBuf[IMG_PAYLOAD_MAX];          // Buffer to hold chunks of image data
  uint32_t sent         = 0;                   // Tracks how many bytes have been sent
  uint16_t img_id       = ++g_image_counter;   // Assign unique ID to this image
  uint16_t total_chunks = (length + IMG_PAYLOAD_MAX - 1) / IMG_PAYLOAD_MAX; // Calculate total chunks
  uint16_t seq          = 0;                   // Sequence number for each chunk

  while (sent < length) {                      // Loop until the whole image has been sent
    size_t toRead = min((size_t)IMG_PAYLOAD_MAX, (size_t)(length - sent)); // Bytes to read in this chunk

    for (size_t i = 0; i < toRead; i++) 
        chunkBuf[i] = SPI.transfer(0x00);      // Read bytes from camera FIFO via SPI

    sendImagePacketWithHeader(chunkBuf, toRead, img_id, seq, total_chunks); // Send chunk via ESP NOW

    sent += toRead;                            // Increment sent byte counter
    seq++;                                     // Increment chunk sequence number
    delay(5);                                  // Small delay to prevent overwhelming the receiver
  }

  myCAM.CS_HIGH();                             // Deselect camera after SPI read
  myCAM.clear_fifo_flag();                     // Reset FIFO flags
  sendImageDone(img_id, total_chunks);         // Notify receiver that full image is sent
}
