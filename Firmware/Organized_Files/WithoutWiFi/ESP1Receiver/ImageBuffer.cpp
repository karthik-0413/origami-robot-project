#include "ImageBuffer.h"
#include "Sensors.h"
#include <Arduino.h>
#include <esp_now.h>
#include <stdlib.h>

// ---------- Sender MAC Addresses ----------
uint8_t sender1MAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C}; // Sender 1
uint8_t sender2MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64}; // Sender 2

// ---------- Image Buffers ----------
ImageBuffer buf1;  // Buffer for sender 1
ImageBuffer buf2;  // Buffer for sender 2


/****************************************************
 * Function: ImageBuffer::reset
 * Description: Frees all allocated memory for image chunks
 *              and resets the buffer state to default.
 * Inputs: None
 * Outputs: None
 * Notes: 
 *    - Frees each chunk's data pointer if allocated.
 *    - Frees the chunk array itself.
 *    - Resets image ID, total chunks, received chunks, 
 *      and last update timestamp.
 ****************************************************/
void ImageBuffer::reset() {
    if (chunks) {
        for (uint16_t i = 0; i < total_chunks; ++i) {
            if (chunks[i].data) {
                free(chunks[i].data);     // Free memory of individual chunk
                chunks[i].data = nullptr; // Nullify pointer after freeing
            }
        }
        free(chunks);                     // Free the array of chunks
        chunks = nullptr;                 // Nullify pointer
    }
    img_id = 0;                           // Reset image ID
    total_chunks = 0;                     // Reset total chunk count
    received_chunks = 0;                  // Reset received chunk count
    lastUpdate = 0;                       // Reset timestamp
}


/****************************************************
 * Function: ImageBuffer::allocChunks
 * Description: Allocates memory for storing image chunks
 *              based on the total number of chunks expected.
 * Inputs: 
 *    - total_chunks: Total number of chunks to allocate.
 * Outputs: 
 *    - Returns true if memory allocation was successful,
 *      false otherwise.
 * Notes: 
 *    - Calls reset() to clear any existing data before allocation.
 *    - Initializes tracking variables for chunk reception.
 ****************************************************/
bool ImageBuffer::allocChunks(uint16_t total_chunks) {
    reset();                                            // Clear any existing image data before allocation
    chunks = (ChunkEntry*)calloc(total_chunks, sizeof(ChunkEntry)); // Allocate memory for all chunks

    if (!chunks) return false;                          // Return false if allocation failed

    this->total_chunks = total_chunks;                  // Store total number of chunks
    received_chunks = 0;                                // Reset count of received chunks
    lastUpdate = millis();                              // Record current time for timeout tracking
    return true;                                        // Allocation successful
}


/****************************************************
 * Function: compareMAC
 * Description: Compares two MAC addresses byte by byte
 *              to determine if they are identical.
 * Inputs:
 *    - mac1: Pointer to the first MAC address (6 bytes)
 *    - mac2: Pointer to the second MAC address (6 bytes)
 * Outputs:
 *    - Returns true if both MAC addresses match,
 *      false otherwise.
 * Notes:
 *    - Used to identify which sender device the data 
 *      originated from.
 ****************************************************/
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    for (int i = 0; i < 6; i++)                             // Loop through each byte of the MAC address
        if (mac1[i] != mac2[i]) return false;               // Return false if any byte differs
    return true;                                             // MAC addresses match
}


/****************************************************
 * Function: macToHex
 * Description: Converts a 6-byte MAC address into a 
 *              readable hexadecimal string format.
 * Inputs:
 *    - mac: Pointer to the MAC address (6 bytes)
 * Outputs:
 *    - Returns a String containing the MAC address
 *      as a 12-character uppercase hexadecimal value.
 * Notes:
 *    - Used for displaying MAC addresses in logs 
 *      and debug messages.
 ****************************************************/
String macToHex(const uint8_t *mac) {
    char tmp[13];
    sprintf(tmp, "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(tmp);
}


/****************************************************
 * Function: tryFinalizeImage
 * Description: Reassembles and outputs a complete image 
 *              once all chunks are received from a sender.
 * Inputs:
 *    - ib: Reference to the ImageBuffer containing chunks.
 *    - senderMac: Pointer to the sender's MAC address.
 * Outputs:
 *    - None (prints image data and status to Serial).
 * Notes:
 *    - Validates image completeness before processing.
 *    - Prints image data between <IMG_START> and <IMG_END> tags.
 *    - Frees buffer memory after forwarding.
 ****************************************************/
// void tryFinalizeImage(ImageBuffer &ib, const uint8_t *senderMac) {

//     // Check if image is incomplete (missing or invalid chunks)
//     if (ib.img_id == 0 || ib.total_chunks == 0 || ib.received_chunks != ib.total_chunks) {
//         Serial.printf("Incomplete image %u from %s. Dropping.\n", ib.img_id, macToHex(senderMac).c_str());
//         ib.reset();  // Clear buffer to prepare for next image
//         return;
//     }

//     // Calculate total image size from all received chunks
//     size_t totalSize = 0;
//     for (uint16_t i = 0; i < ib.total_chunks; ++i) totalSize += ib.chunks[i].len;

//     // Construct header message for image start metadata
//     String header = "<IMG_START:" + macToHex(senderMac) + ":" + 
//                     String(ib.img_id) + ":" + 
//                     String((unsigned long)totalSize) + ">\n";
//     Serial.print(header);

//     // Write each chunk's data sequentially to the serial output
//     for (uint16_t i = 0; i < ib.total_chunks; ++i) {
//         if (ib.chunks[i].len > 0 && ib.chunks[i].data) {
//             Serial.write(ib.chunks[i].data, ib.chunks[i].len);
//         }
//     }

//     // Print end marker and log finalization info
//     Serial.print("\n<IMG_END>\n");
//     Serial.printf("Image %u from %s forwarded (size=%u bytes)\n", 
//                   ib.img_id, macToHex(senderMac).c_str(), (unsigned)totalSize);

//     ib.reset();  // Free all allocated memory for this image
// }
unsigned long lastTimeSender1 = 0;
unsigned long lastTimeSender2 = 0;

void tryFinalizeImage(ImageBuffer &ib, const uint8_t *senderMac) {
    // Check if image is incomplete
    if (ib.img_id == 0 || ib.total_chunks == 0 || ib.received_chunks != ib.total_chunks) {
        Serial.printf("Incomplete image %u from %s. Dropping.\n", ib.img_id, macToHex(senderMac).c_str());
        ib.reset();  // Prepare buffer for next image
        return;
    }

    unsigned long now = millis();
    float fps = 0;

    if (compareMAC(senderMac, sender1MAC)) {
        if (lastTimeSender1 > 0) fps = 1000.0 / (now - lastTimeSender1);
        lastTimeSender1 = now;
    } else if (compareMAC(senderMac, sender2MAC)) {
        if (lastTimeSender2 > 0) fps = 1000.0 / (now - lastTimeSender2);
        lastTimeSender2 = now;
    }

    // Print FPS instead of image
    Serial.printf("Camera %s FPS: %.2f\n", macToHex(senderMac).c_str(), fps);

    ib.reset();  // Free memory for next image
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

    /*********************** CASE A: DONE packet ************************
     * Indicates the sender has finished sending all image chunks.
     * Contains image ID and total number of chunks for final validation.
     *******************************************************************/
    if (len >= 5 && data[0] == 'D') {
        uint16_t img_id = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
        uint16_t total_chunks = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
        ImageBuffer *ib = nullptr;

        // Identify which sender the packet came from
        if (compareMAC(mac, sender1MAC)) ib = &buf1;
        else if (compareMAC(mac, sender2MAC)) ib = &buf2;

        // Finalize image if the IDs match
        if (ib && ib->img_id == img_id) {
            ib->total_chunks = total_chunks;
            tryFinalizeImage(*ib, mac);
        }
        return;
    }

    /********************** CASE B: Image chunk packet *******************
     * Contains a portion of an image being transmitted.
     * Each chunk includes metadata: image ID, sequence number, and total chunks.
     *******************************************************************/
    if (len >= 7 && data[0] == 'I') {
        // Extract header metadata
        uint16_t img_id = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
        uint16_t seq = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
        uint16_t total = (uint16_t)data[5] | ((uint16_t)data[6] << 8);
        const uint8_t *payload = data + 7;      // Image chunk data
        int payloadLen = len - 7;

        ImageBuffer *ib = nullptr;
        if (compareMAC(mac, sender1MAC)) ib = &buf1;
        else if (compareMAC(mac, sender2MAC)) ib = &buf2;
        else return;  // Unknown sender

        // If this is a new image, allocate buffers
        if (ib->img_id != img_id) {
            // If previous image was incomplete, reset it
            if (ib->img_id != 0 && (now - ib->lastUpdate) < IMAGE_TIMEOUT_MS) {
                Serial.printf("New img_id %u from %s while previous %u incomplete. Resetting.\n",
                              img_id, macToHex(mac).c_str(), ib->img_id);
            }

            // Allocate chunk memory
            if (!ib->allocChunks(total == 0 ? 200 : total)) {
                Serial.println("Failed to alloc image chunks. Dropping.");
                return;
            }

            ib->img_id = img_id;
            ib->lastUpdate = now;
        }

        // Ignore invalid or duplicate chunks
        if (seq >= ib->total_chunks) return;
        if (ib->chunks[seq].data != nullptr) {
            ib->lastUpdate = now;
            return;
        }

        // Allocate memory for this chunk
        ib->chunks[seq].data = (uint8_t*)malloc(payloadLen);
        if (!ib->chunks[seq].data) {
            Serial.println("OOM allocating chunk. Dropping buffer.");
            ib->reset();
            return;
        }

        // Copy chunk data and update stats
        memcpy(ib->chunks[seq].data, payload, payloadLen);
        ib->chunks[seq].len = payloadLen;
        ib->received_chunks++;
        ib->lastUpdate = now;

        // If all chunks received, finalize image
        if (ib->received_chunks == ib->total_chunks) 
            tryFinalizeImage(*ib, mac);
        return;
    }

    /*********************** CASE C: Sensor data packet *******************
     * Contains sensor readings from one of the sender nodes.
     * Populates the corresponding SensorPacket struct.
     ********************************************************************/
    if (compareMAC(mac, sender1MAC) && len == sizeof(SensorPacket1)) 
        memcpy(&packet1, data, sizeof(SensorPacket1));
    else if (compareMAC(mac, sender2MAC) && len == sizeof(SensorPacket2)) 
        memcpy(&packet2, data, sizeof(SensorPacket2));
}
