#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <ArduCAM.h>
#include "memorysaver.h"

// Define constants used for splitting and sending images
#define CHUNK_SIZE 200                // Total size of each chunk to send over ESP-NOW
#define IMG_HEADER_SIZE 7             // Number of bytes reserved for image header in each chunk
#define IMG_PAYLOAD_MAX (CHUNK_SIZE - IMG_HEADER_SIZE) // Maximum data bytes in each chunk after header
#define CS 5                          // Chip Select pin for SPI communication with camera

// Declare the global camera object (defined in source file)
// extern -> Declare a variable that exists somewhere else
extern ArduCAM myCAM;                  // ArduCAM object for OV2640 camera

// Function declarations
void initCamera();                     // Initializes camera and sets resolution/format
void captureAndSend();                 // Captures an image and sends it in chunks over ESP-NOW

#endif  // CAMERA_MODULE_H