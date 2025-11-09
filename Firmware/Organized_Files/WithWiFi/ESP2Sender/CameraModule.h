#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <ArduCAM.h>
#include <WiFi.h>
#include "memorysaver.h"

#define CHUNK_SIZE 200
#define IMG_HEADER_SIZE 7
#define IMG_PAYLOAD_MAX (CHUNK_SIZE - IMG_HEADER_SIZE)
#define CS 5

extern ArduCAM myCAM;

void initCamera();
void captureAndSend(WiFiClient* client);  // TCP version now accepts client pointer

#endif  // CAMERA_MODULE_H
