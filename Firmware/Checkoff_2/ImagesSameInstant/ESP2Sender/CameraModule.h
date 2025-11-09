#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <ArduCAM.h>
#include "memorysaver.h"

// Define chip select pin for SPI communication with camera
#define CS 5

// Declare the global camera object (defined in source file)
extern ArduCAM myCAM;

// Function declarations
void initCamera();         // Initializes camera and sets resolution/format
void captureAndSend();     // Captures an image and sends it via UDP

#endif  // CAMERA_MODULE_H