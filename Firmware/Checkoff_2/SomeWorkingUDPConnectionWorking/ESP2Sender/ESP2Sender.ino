#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"

// Flag set when receiver triggers a capture
volatile bool captureFlag = false;

/****************************************************
 * Function: onTriggerReceived
 * Description: ESP-NOW callback when a trigger is received
 ****************************************************/
void onTriggerReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len <= 0) return;

    // Optional: check MAC if you want to verify it's from receiver
    Serial.println("Trigger received!");
    
    // Capture and send image
    captureAndSend();
}


void setup() {
    Serial.begin(115200);
    Wire.begin();

    initCamera();       // ArduCAM init
    initWiFi();         // WiFi for UDP
    initESPNow();       // ESP-NOW for triggers

    // Register ESP-NOW receive callback for triggers
    esp_now_register_recv_cb(onTriggerReceived);

    Serial.println("Sender initialized");
}

void loop() {
    if (captureFlag) {
        captureAndSend(); // Capture image and send via UDP
        captureFlag = false;
    }

    delay(10); // Small delay for stability
}
