#include "Comms.h"

// MAC Address of the receiver ESP32 for ESP-NOW (sensor data only)
uint8_t receiverMAC[] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};

// WiFi UDP client for image transfer
WiFiUDP udpClient;

/****************************************************
 * Function: OnDataSent
 * Description: Callback function that is executed after 
 *              an ESP-NOW packet has been sent.
 * Inputs: 
 *    - info: Pointer to wifi_tx_info_t structure
 *    - status: esp_now_send_status_t indicating success/fail
 * Outputs: None
 * Notes: Currently empty, can be extended for logging
 ***************************************************/
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Optional: Add logging here if needed
}

/****************************************************
 * Function: OnDataRecv
 * Description: Callback when ESP-NOW data is received.
 *              Listens for capture command from receiver.
 * Inputs:
 *    - info: Receive info containing sender MAC
 *    - data: Received data
 *    - len: Length of data
 * Outputs: None
 * Notes: Triggers image capture when command received
 ***************************************************/
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == 1 && data[0] == 0xC4) {  // Capture command
    Serial.println("Capture command received!");
    captureAndSend();  // Trigger immediate capture (blocking is fine)
  }
}

/****************************************************
 * Function: initWiFi
 * Description: Connects ESP32 to WiFi router for UDP
 *              image transmission with static IP.
 * Inputs: None
 * Outputs: None
 * Notes: 
 *    - Uses WIFI_AP_STA mode to support both WiFi and ESP-NOW
 *    - Configures static IP before connecting
 *    - Waits up to 10 seconds for connection
 *    - Prints connection status to Serial
 ****************************************************/
void initWiFi() {
  WiFi.mode(WIFI_AP_STA);  // Both Station (for WiFi) + AP (for ESP-NOW compatibility)
  
  // Configure static IP for this sender
  // CAMERA_ID 1 = 192.168.8.101, CAMERA_ID 2 = 192.168.8.102
  IPAddress local_IP(192, 168, 8, 100 + CAMERA_ID);  // .101 or .102
  IPAddress gateway(192, 168, 8, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 8, 1);
  
  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("Static IP configuration failed!");
  }
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("Static IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Receiver IP: ");
    Serial.println(RECEIVER_IP);
    Serial.println("Using UDP for image transfer");
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Check SSID and password in Comms.h");
  }
}

/****************************************************
 * Function: initESPNow
 * Description: Initializes ESP-NOW for sending sensor data
 *              and receiving capture commands.
 * Inputs: None
 * Outputs: None
 * Notes: 
 *    - WiFi must be initialized first (initWiFi)
 *    - Registers both send and receive callbacks
 *    - Adds receiver as peer
 *    - Halts execution if initialization fails
 ****************************************************/
void initESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1);
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);  // Register receive callback for capture commands

  // Configure the peer (receiver)
  esp_now_peer_info_t peerInfo = {};                  
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;                    // Use default WiFi channel
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer!");
    while (1);
  }
  
  Serial.println("ESP-NOW initialized for sensor data and capture commands");
}

/****************************************************
 * Function: sendImageUDP
 * Description: Sends complete image via UDP in chunks.
 * Inputs: 
 *    - imageData: Pointer to JPEG image buffer
 *    - length: Size of image in bytes
 * Outputs:
 *    - Returns true if all chunks sent successfully
 * Notes:
 *    - Chunk format: [CAMERA_ID(1)][SEQ(2)][TOTAL(2)][DATA]
 *    - UDP max safe payload: 1400 bytes
 *    - Non-blocking sends for consistent timing
 ****************************************************/
bool sendImageUDP(uint8_t* imageData, uint32_t length) {
  const size_t UDP_PAYLOAD = 1400;  // Safe UDP payload size (< 1472 MTU)
  const size_t HEADER_SIZE = 5;      // Camera ID (1) + Seq (2) + Total (2)
  
  // Calculate total number of chunks needed
  uint16_t totalChunks = (length + UDP_PAYLOAD - 1) / UDP_PAYLOAD;
  
  // Allocate packet buffer on heap (not stack!)
  uint8_t *packet = (uint8_t*)malloc(HEADER_SIZE + UDP_PAYLOAD);
  if (!packet) {
    Serial.println("Failed to allocate UDP packet buffer");
    return false;
  }
  
  // Send each chunk
  for (uint16_t seq = 0; seq < totalChunks; seq++) {
    size_t offset = seq * UDP_PAYLOAD;
    size_t chunkSize = min((size_t)UDP_PAYLOAD, (size_t)(length - offset));
    
    // Header: [CAMERA_ID(1)][SEQ(2)][TOTAL(2)]
    packet[0] = CAMERA_ID;
    packet[1] = (seq >> 8) & 0xFF;        // Seq high byte
    packet[2] = seq & 0xFF;               // Seq low byte
    packet[3] = (totalChunks >> 8) & 0xFF; // Total high byte
    packet[4] = totalChunks & 0xFF;       // Total low byte
    
    // Copy image data after header
    memcpy(&packet[HEADER_SIZE], &imageData[offset], chunkSize);
    
    // Send UDP packet
    udpClient.beginPacket(RECEIVER_IP, IMAGE_PORT);
    size_t written = udpClient.write(packet, HEADER_SIZE + chunkSize);
    udpClient.endPacket();
    
    if (written != HEADER_SIZE + chunkSize) {
      Serial.printf("UDP send failed for chunk %u\n", seq);
      free(packet);
      return false;
    }
    
    yield();  // Let other tasks run between chunks
  }
  
  free(packet);  // Clean up
  Serial.printf("Sent %u bytes via UDP (%u chunks)\n", length, totalChunks);
  return true;
}

/****************************************************
 * Function: sendSensorData
 * Description: Sends IR & IMU sensor data via ESP-NOW.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Uses global sensorData structure
 *    - ESP-NOW is ideal for small, frequent sensor updates
 *    - Much lower latency than UDP for small packets
 ****************************************************/
void sendSensorData() {
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&sensorData, sizeof(sensorData));
  
  if (result != ESP_OK) {
    Serial.println("ESP-NOW send failed");
  }
}