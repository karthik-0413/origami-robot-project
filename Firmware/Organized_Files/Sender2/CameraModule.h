#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <Arduino.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

// Camera CS pin
#define CS 5

extern ArduCAM myCAM;

void initCamera();
void captureAndSend();

#endif
