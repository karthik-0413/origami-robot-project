#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"

/****************************************************
 * Function: setup
 * Description: Initializes all hardware components
 *              and communication protocols.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Starts Serial for debugging
 *    - Initializes I2C (Wire) bus for sensors
 *    - Initializes camera, distance sensors, IMU, and ESP-NOW
 ****************************************************/
void setup() {
  Serial.begin(115200);    // Start serial monitor at 115200 baud
  Wire.begin();            // Initialize I2C bus

  initCamera();            // Initialize ArduCAM
  initVL53L0X();           // Initialize VL53L0X distance sensors
  initMPU();               // Initialize MPU6050 accelerometer/gyro
  initESPNow();            // Initialize ESP-NOW and configure receiver
}

/****************************************************
 * Function: loop
 * Description: Main program loop; repeatedly reads
 *              sensor data, captures images, and sends
 *              data over ESP-NOW.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Reads IMU data every ~50ms (handled internally)
 *    - Captures camera image and sends in chunks
 *    - Reads IR sensors and updates sensorData
 *    - Sends sensor data to receiver
 ****************************************************/
void loop() {
  readMPU();               // Read accelerometer and gyroscope
  captureAndSend();        // Capture camera image and send in chunks
  readVL53L0X();           // Read distance sensors and update sensorData
  sendSensorData();        // Send current sensor data via ESP-NOW
}