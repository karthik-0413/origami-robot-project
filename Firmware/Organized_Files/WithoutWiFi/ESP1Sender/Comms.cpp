#include "Comms.h"

// MAC Address of the Middle-Man ESP32
uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};

/****************************************************
 * Function: OnDataSent
 * Description: Callback function that is executed after 
 *              an ESP-NOW packet has been sent. Can be 
 *              used to check whether the transmission 
 *              was successful or failed.
 * Inputs: 
 *    - info: Pointer to wifi_tx_info_t structure containing 
 *            information about the sent packet
 *    - status: esp_now_send_status_t indicating whether 
 *              the packet was sent successfully (ESP_NOW_SEND_SUCCESS)
 *              or failed (ESP_NOW_SEND_FAIL)
 * Outputs: None
 * Notes: Currently empty, but can be extended to log 
 *        transmission status or retry failed packets.
 ***************************************************/
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}


/****************************************************
 * Function: initESPNow
 * Description: Initializes ESP-NOW on the ESP32 and 
 *              configures a peer device to send data to.
 * Inputs: None
 * Outputs: None
 * Notes: 
 *    - Sets WiFi mode to Station (required for ESP-NOW).
 *    - Registers OnDataSent callback to track transmission status.
 *    - Configures peer information including MAC address, 
 *      channel, and encryption.
 *    - Halts execution if ESP-NOW initialization or adding 
 *      the peer fails.
 ****************************************************/
void initESPNow() {
  WiFi.mode(WIFI_STA);                                  // Set WiFi to Station mode
  if (esp_now_init() != ESP_OK) while (1);              // Initialize ESP-NOW, halt on failure

  esp_now_register_send_cb(OnDataSent);                 // Register callback for sent packets

  // Configure the peer (receiver)
  esp_now_peer_info_t peerInfo = {};                  
  memcpy(peerInfo.peer_addr, receiverMAC, 6);           // Set MAC address of receiver
  peerInfo.channel = 0;                                 // Use default WiFi channel
  peerInfo.encrypt = false;                             // Disable encryption
  if (esp_now_add_peer(&peerInfo) != ESP_OK) while (1); // Add peer, halt on failure
}


/****************************************************
 * Function: sendSensorData
 * Description: Sends the current IR & IMU sensor data structure 
 *              over ESP-NOW to a pre-configured receiver.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - sensorData must be defined elsewhere (global struct or variable)
 *    - Casts the sensorData structure to a byte array for transmission
 *    - Uses esp_now_send to send the full size of sensorData
 *    - Can be used for periodic sensor updates or real-time monitoring
 ****************************************************/
void sendSensorData() {
  esp_now_send(receiverMAC, (uint8_t*)&sensorData, sizeof(sensorData));
}


/****************************************************
 * Function: sendImagePacketWithHeader
 * Description: Sends a single chunk of an image over ESP-NOW,
 *              including a header containing image ID, sequence
 *              number, and total number of chunks.
 * Inputs: 
 *    - payload: Pointer to the image data to send in this chunk
 *    - payloadLen: Length of the payload in bytes
 *    - img_id: Unique ID of the image being sent
 *    - seq: Sequence number of this chunk (0-based)
 *    - total: Total number of chunks for this image
 * Outputs:
 *    - Returns true if the chunk was successfully sent
 *    - Returns false if sending failed after 2 attempts
 * Notes:
 *    - Packet format: [Header(7 bytes) | Payload]
 *      Header layout:
 *        byte 0: 'I' to indicate image packet
 *        bytes 1-2: img_id (little-endian)
 *        bytes 3-4: seq (little-endian)
 *        bytes 5-6: total (little-endian)
 *    - Retries sending the packet twice if first attempt fails
 *    - Uses a short delay between retries to avoid congestion
 ****************************************************/
bool sendImagePacketWithHeader(uint8_t *payload, size_t payloadLen,
                               uint16_t img_id, uint16_t seq, uint16_t total) {
  const uint8_t IMG_HEADER_SIZE = 7;               // Number of bytes reserved for packet header
  const size_t CHUNK_SIZE = 200;                   // Total packet size including header

  uint8_t packet[CHUNK_SIZE];                      // Buffer to store header + payload

  // Construct header
  packet[0] = 'I';                                 // 'I' = image packet
  packet[1] = img_id & 0xFF;                       // Lower byte of image ID
  packet[2] = (img_id >> 8) & 0xFF;                // Upper byte of image ID
  packet[3] = seq & 0xFF;                          // Lower byte of sequence number
  packet[4] = (seq >> 8) & 0xFF;                   // Upper byte of sequence number
  packet[5] = total & 0xFF;                        // Lower byte of total chunks
  packet[6] = (total >> 8) & 0xFF;                 // Upper byte of total chunks

  // Copy image data into packet buffer after header
  memcpy(&packet[IMG_HEADER_SIZE], payload, payloadLen);
  size_t sendLen = IMG_HEADER_SIZE + payloadLen;   // Total bytes to send

  // Attempt to send packet up to 2 times
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (esp_now_send(receiverMAC, packet, sendLen) == ESP_OK)
        return true;                               // Success
    delay(4);                                      // Short delay before retry
  }
  return false;                                    // Failed after 2 attempts
}


/****************************************************
 * Function: sendImageDone
 * Description: Sends a "done" notification over ESP-NOW 
 *              to indicate that all chunks of a given 
 *              image have been transmitted successfully.
 * Inputs: 
 *    - img_id: Unique ID of the image that has been fully sent
 *    - total: Total number of chunks for the image
 * Outputs: None
 * Notes:
 *    - Packet format: [Header(5 bytes)]
 *      Header layout:
 *        byte 0: 'D' to indicate "done" packet
 *        bytes 1-2: img_id (little-endian)
 *        bytes 3-4: total number of chunks (little-endian)
 *    - Sends the packet to the receiver MAC address using ESP-NOW
 *    - Can be used by the receiver to know when to reconstruct the full image
 ****************************************************/
void sendImageDone(uint16_t img_id, uint16_t total) {
  uint8_t buf[5];                  // Buffer for the "done" packet

  buf[0] = 'D';                    // 'D' = done packet
  buf[1] = img_id & 0xFF;          // Lower byte of image ID
  buf[2] = (img_id >> 8) & 0xFF;   // Upper byte of image ID
  buf[3] = total & 0xFF;           // Lower byte of total chunks
  buf[4] = (total >> 8) & 0xFF;    // Upper byte of total chunks

  esp_now_send(receiverMAC, buf, sizeof(buf));  // Send done notification
}
