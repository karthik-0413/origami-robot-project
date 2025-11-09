#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Sensors.h"

// MAC address of the receiver ESP device
// Must be defined in a source file
extern uint8_t receiverMAC[];

// Function Declarations
void initESPNow();                                                              // Initialize ESP-NOW and configure receiver peer
bool sendImagePacketWithHeader(uint8_t *payload, size_t payloadLen,             // Send a single image chunk with header
                               uint16_t img_id, uint16_t seq, uint16_t total);
void sendImageDone(uint16_t img_id, uint16_t total);                            // Notify receiver that all image chunks are sent
void sendSensorData();                                                          // Send current IR & IMU sensor data structure over ESP-NOW


#endif  // COMMS_H
