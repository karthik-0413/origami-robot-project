#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <ArduCAM.h>
#include "memorysaver.h"
#include <Arduino.h>
#include <Wire.h>

#define CS 5
extern ArduCAM myCAM;
extern TwoWire I2C_CAM;  // Declare external I2C bus

void initCamera();     
void captureAndSend();

#endif
