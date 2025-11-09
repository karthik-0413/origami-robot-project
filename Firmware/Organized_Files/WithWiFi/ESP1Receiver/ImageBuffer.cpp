#include "ImageBuffer.h"
#include "Sensors.h"
#include <Arduino.h>
#include <esp_now.h>
#include <stdlib.h>

// ---------- Sender MAC Addresses ----------
uint8_t sender1MAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C}; // Sender 1
uint8_t sender2MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64}; // Sender 2

// ---------- Image Buffers ----------
ImageBuffer tcpBuffer1;  // Buffer for sender 1
ImageBuffer tcpBuffer2;  // Buffer for sender 2


void ImageBuffer::reset() {
    img_id = 0;
}

/****************************************************
 * Function: tryFinalizeImageTCP
 * Description: Prints a TCP image to Serial in the same
 *              format as ESP-NOW images.
 ****************************************************/
void tryFinalizeImageTCP(uint8_t* data, size_t length, int clientIndex) {
    // clientIndex = 0 → tcpBuffer1, clientIndex = 1 → tcpBuffer2
    ImageBuffer* buf = (clientIndex == 0) ? &tcpBuffer1 : &tcpBuffer2;

    String header = "<IMG_START:TCP:" + String(++buf->img_id) + ":" + String((unsigned long)length) + ">\n";
    Serial.print(header);
    Serial.write(data, length);
    Serial.print("\n<IMG_END>\n");
    Serial.printf("TCP Image %u forwarded (size=%u bytes)\n", buf->img_id, (unsigned)length);
}


bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    for (int i = 0; i < 6; i++) // Loop through each byte of the MAC address
    if (mac1[i] != mac2[i]) return false; // Return false if any byte differs
    return true; // MAC addresses match
}


/****************************************************
 * Function: OnDataRecv
 * Description: Handles all incoming ESP-NOW packets from senders.
 *              Distinguishes between image data, completion signals,
 *              and sensor data, processing each appropriately.
 * Inputs:
 *    - info: Pointer to the ESP-NOW receive info structure (contains sender MAC).
 *    - data: Pointer to the received data buffer.
 *    - len:  Length of the received data in bytes.
 * Outputs:
 *    - None (processes data internally and forwards images via Serial).
 * Notes:
 *    - CASE A: 'D' packets signal completion of an image transfer.
 *    - CASE B: 'I' packets contain image data chunks.
 *    - CASE C: Other packets contain sensor data.
 ****************************************************/
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t *mac = info->src_addr;       // Extract sender's MAC address
    unsigned long now = millis();              // Record current timestamp
    if (len <= 0) return;                      // Ignore empty or invalid packets

    /*********************** CASE C: Sensor data packet *******************
     * Contains sensor readings from one of the sender nodes.
     * Populates the corresponding SensorPacket struct.
     ********************************************************************/
    if (compareMAC(mac, sender1MAC) && len == sizeof(SensorPacket1)) 
        memcpy(&packet1, data, sizeof(SensorPacket1));
    else if (compareMAC(mac, sender2MAC) && len == sizeof(SensorPacket2)) 
        memcpy(&packet2, data, sizeof(SensorPacket2));
}
