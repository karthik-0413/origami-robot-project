#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"

void setup() {
  Serial.begin(115200);
  Wire.begin();

  initCamera();
  initVL53L0X();
  initMPU();
  initESPNow();
}

void loop() {
  readMPU();
  captureAndSend();
  readVL53L0X();
  sendSensorData();
}
