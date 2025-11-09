#include "Comms.h"
#include "ImageBuffer.h"
#include <WiFi.h>
#include <esp_now.h>

namespace Comms {

/****************************************************
 * Function: initESPNow
 * Description: Initializes the ESP-NOW communication 
 *              protocol and sets the WiFi mode to STA.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Prints error message and halts if initialization fails.
 ****************************************************/
void initESPNow() {
    WiFi.mode(WIFI_STA);                                       // Set WiFi mode to Station (no AP mode)
    if (esp_now_init() != ESP_OK) {                            // Initialize ESP-NOW and check for success
        Serial.println("ESP-NOW init failed!");                // Print error message
        while (1);                                             // Halt program on failure
    }
    Serial.println("ESP-NOW initialized.");                    // Confirmation message on success
}

/****************************************************
 * Function: registerCallback
 * Description: Registers the callback function to handle
 *              incoming ESP-NOW data packets.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Links the OnDataRecv function from ImageBuffer.cpp
 *      to the ESP-NOW receive event handler.
 ****************************************************/
void registerCallback() {
    esp_now_register_recv_cb(OnDataRecv);                      // Register callback for receiving data
}

} // namespace Comms
