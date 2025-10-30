#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <ArduCAM.h>
#include "memorysaver.h"

// ---------- Camera Config ----------
#define CHUNK_SIZE 200
#define IMG_HEADER_SIZE 7
#define IMG_PAYLOAD_MAX (CHUNK_SIZE - IMG_HEADER_SIZE)
#define CS 5

extern ArduCAM myCAM;

// Functions
void initCamera();
void captureAndSend();

#endif
