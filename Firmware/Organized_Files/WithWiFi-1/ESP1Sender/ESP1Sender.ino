#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "CameraModule.h"
// #include "Sensors.h"  // Commented out - camera only
#include "Comms.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Wire.begin();  // Commented out - no sensors
  
  initCamera();            // Initialize ArduCAM only
  
  // initVL53L0X();        // Commented out
  // initMPU();            // Commented out
  
  initWiFi();              // Initialize WiFi for UDP image transfer
  initESPNow();            // Initialize ESP-NOW for capture trigger
  
  Serial.println("Sender ready - camera only mode");
}

void loop() {
  // Camera capture is triggered by ESP-NOW command from receiver
  // No other processing in loop to maximize responsiveness
  
  // readMPU();            // Commented out
  // readVL53L0X();        // Commented out
  // sendSensorData();     // Commented out
  
  yield();  // Allow ESP-NOW callbacks to process
}