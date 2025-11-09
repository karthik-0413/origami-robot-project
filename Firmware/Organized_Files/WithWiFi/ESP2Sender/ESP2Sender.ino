#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"
#include "WiFiConfig.h"   // Include this instead of declaring externs manually

void setup() {
    Serial.begin(115200);
    Wire.begin();

    // Initialize Camera, Sensors, ESP-NOW
    initCamera();
    initVL53L0X();
    initESPNow();

    // Initialize Wi-Fi TCP for images
    initWiFiTCP();
}

void loop() {
    captureAndSend(&tcpClient);     // Capture and send image via TCP
    readVL53L0X();                  // Read distance sensors
    sendSensorData();               // Send sensor data via ESP-NOW
}
