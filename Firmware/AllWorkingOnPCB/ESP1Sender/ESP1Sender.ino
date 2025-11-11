#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"

// ---------- Trigger flag ----------
volatile bool triggerReceived = false;

// ---------- Trigger ACK ----------
uint8_t ackMsg = 1;

// ---------- ESP-NOW receive callback ----------
void onTriggerReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == 1 && data[0] == 1) {
        triggerReceived = true;
        // Serial.println("Trigger received!");
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    initCamera();          // Initialize camera
    initWiFi();            // Initialize WiFi (UDP)
    initESPNow();          // Initialize ESP-NOW
    initVL53L0X();        // Initialize 4 VL53L0X sensors with unique addresses
    initMPU();            // Initialize MPU6050 accelerometer/gyroscope

    // Register ESP-NOW receive callback
    esp_now_register_recv_cb(onTriggerReceived);

    Serial.println("Sender initialized");
}

void loop() {
    readVL53L0X();        // Read distance data from 4 VL53L0X sensors
    readMPU();            // Read accelerometer and gyroscope data from MPU6050
    sendSensorData();
    // Only capture and send image if trigger was received
    if (triggerReceived) {
        triggerReceived = false; // Reset trigger flag

        captureAndSend();        // Capture and send via UDP

        // Send ACK to receiver
        esp_err_t res = esp_now_send(receiverMAC, &ackMsg, 1);
        if (res == ESP_OK) {
            Serial.println("ACK sent to receiver");
        } else {
            Serial.println("Failed to send ACK");
        }

        delay(50);  // Short delay to avoid ESP-NOW congestion
    }

    delay(10); // Small loop delay
}
