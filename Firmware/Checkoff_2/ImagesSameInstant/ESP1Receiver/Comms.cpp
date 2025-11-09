#include "Comms.h"
#include "ImageBuffer.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>

WiFiUDP udpServer;
WiFiUDP forwardClient;  // For forwarding images to laptop

// Laptop configuration
const char* LAPTOP_IP = "192.168.8.100";
const int LAPTOP_PORT = 9999;

// Sender MAC addresses (must match senders)
uint8_t sender1MAC[6] = {0x14, 0x33, 0x5C, 0x02, 0x88, 0x34};  // Sender 1
uint8_t sender2MAC[6] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64};  // Sender 2

namespace Comms {

/****************************************************
 * Function: initWiFi
 * Description: Connects receiver to WiFi network with static IP.
 ****************************************************/
void initWiFi() {
    WiFi.mode(WIFI_AP_STA);  // Both Station (for WiFi) + AP (for ESP-NOW)
    
    // Configure static IP for receiver
    IPAddress local_IP(192, 168, 8, 103);  // Receiver gets .103
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
        Serial.println("Expecting sender IPs: 192.168.8.101 and 192.168.8.102");
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

/****************************************************
 * Function: initUDPServer
 * Description: Starts UDP server for receiving images.
 ****************************************************/
void initUDPServer() {
    udpServer.begin(IMAGE_PORT);
    Serial.printf("UDP Server started on port %d\n", IMAGE_PORT);
}

/****************************************************
 * Function: handleUDPPackets
 * Description: Handles incoming UDP packets containing image chunks.
 * Notes:
 *    - Packet format: [CAMERA_ID(1)][SEQ(2)][TOTAL(2)][DATA]
 *    - Reassembles chunks and processes complete images
 ****************************************************/
void handleUDPPackets() {
    int packetSize = udpServer.parsePacket();
    if (packetSize == 0) return;  // No packet available
    
    // Read packet
    uint8_t packet[1500];  // Max UDP packet size
    int len = udpServer.read(packet, sizeof(packet));
    
    if (len < 5) {
        Serial.println("Invalid packet: too short");
        return;  // Need at least 5-byte header
    }
    
    // Parse header: [CAMERA_ID(1)][SEQ(2)][TOTAL(2)]
    uint8_t camera_id = packet[0];
    uint16_t seq = ((uint16_t)packet[1] << 8) | packet[2];
    uint16_t total = ((uint16_t)packet[3] << 8) | packet[4];
    const uint8_t *payload = &packet[5];
    int payloadLen = len - 5;
    
    // Validate camera ID
    if (camera_id != 1 && camera_id != 2) {
        Serial.printf("Invalid camera_id: %u\n", camera_id);
        return;
    }
    
    // Select buffer based on camera ID
    ImageBuffer *ib = (camera_id == 1) ? &buf1 : &buf2;
    
    // If this is a new image, allocate chunks
    if (ib->img_id == 0 || ib->total_chunks == 0) {
        if (!ib->allocChunks(total)) {
            Serial.println("Failed to allocate chunks");
            return;
        }
        ib->camera_id = camera_id;
        ib->img_id++;  // Increment image counter
        Serial.printf("Camera %u - Starting image %u (%u chunks expected)\n", 
                      camera_id, ib->img_id, total);
    }
    
    // Validate sequence number
    if (seq >= ib->total_chunks) {
        Serial.printf("Invalid seq %u (total %u)\n", seq, ib->total_chunks);
        return;
    }
    
    // Check if chunk already received (duplicate)
    if (ib->chunks[seq].data != nullptr) {
        ib->lastUpdate = millis();  // Update timestamp but don't store duplicate
        return;
    }
    
    // Allocate memory for this chunk
    ib->chunks[seq].data = (uint8_t*)malloc(payloadLen);
    if (!ib->chunks[seq].data) {
        Serial.println("OOM allocating chunk");
        ib->reset();
        return;
    }
    
    // Store chunk data
    memcpy(ib->chunks[seq].data, payload, payloadLen);
    ib->chunks[seq].len = payloadLen;
    ib->received_chunks++;
    ib->lastUpdate = millis();
    
    // Check if image is complete
    if (ib->received_chunks == ib->total_chunks) {
        finalizeImage(*ib);
    }
}

/****************************************************
 * Function: initESPNow
 * Description: Initializes ESP-NOW and adds sender peers.
 ****************************************************/
void initESPNow() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        while (1);
    }
    
    // Add sender 1 as peer
    esp_now_peer_info_t peer1 = {};
    memcpy(peer1.peer_addr, sender1MAC, 6);
    peer1.channel = 0;
    peer1.encrypt = false;
    if (esp_now_add_peer(&peer1) != ESP_OK) {
        Serial.println("Failed to add Sender 1!");
    }
    
    // Add sender 2 as peer
    esp_now_peer_info_t peer2 = {};
    memcpy(peer2.peer_addr, sender2MAC, 6);
    peer2.channel = 0;
    peer2.encrypt = false;
    if (esp_now_add_peer(&peer2) != ESP_OK) {
        Serial.println("Failed to add Sender 2!");
    }
    
    Serial.println("ESP-NOW initialized with both senders as peers");
}

/****************************************************
 * Function: sendCaptureCommand
 * Description: Sends capture trigger to both senders via ESP-NOW.
 ****************************************************/
void sendCaptureCommand() {
    uint8_t command = 0xC4;  // 'C' for capture (0xC4 = 196)
    
    // Send to both senders simultaneously
    esp_now_send(sender1MAC, &command, 1);
    esp_now_send(sender2MAC, &command, 1);
    
    Serial.println("Capture command sent to both senders");
}

/****************************************************
 * Function: registerCallback
 * Description: Registers ESP-NOW callback for sensor data.
 ****************************************************/
void registerCallback() {
    esp_now_register_recv_cb(OnDataRecv);
}

} // namespace Comms