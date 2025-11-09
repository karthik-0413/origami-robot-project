#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <ArduCAM.h>
#include "memorysaver.h"

#define CS 5

extern ArduCAM myCAM;

void initCamera();
void captureAndSend();

#endif