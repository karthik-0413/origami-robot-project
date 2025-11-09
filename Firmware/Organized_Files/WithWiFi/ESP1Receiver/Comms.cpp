#include "Comms.h"
#include "ImageBuffer.h"
#include <WiFi.h>
#include <esp_now.h>

namespace Comms {

// TCP server for image reception
WiFiServer tcpServer(5000);
WiFiClient tcpClient;

// Keep track of connected clients (max 2 ESPs)
WiFiClient clients[2]; 

// ---------- ESP-NOW ----------
void initESPNow() {
    WiFi.mode(WIFI_STA);  // Station mode for ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        while (1);
    }
    Serial.println("ESP-NOW initialized.");
}

void registerCallback() {
    esp_now_register_recv_cb(OnDataRecv);  // Register callback
}

// ---------- Wi-Fi TCP ----------
void initWiFiTCP(uint16_t port) {
    tcpServer = WiFiServer(port);

    // Configure static IP
    IPAddress local_IP(192, 168, 8, 102);  // Receiver ESP
    IPAddress gateway(192, 168, 8, 1);
    IPAddress subnet(255, 255, 255, 0);
    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }

    const char* WIFI_SSID = "GL-MT3000-8d3";
    const char* WIFI_PASSWORD = "QYAXW83ASS";

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected");

    tcpServer.begin();
    Serial.println("TCP server started");
}


void handleTCPClients() {
    // Accept new clients if slots are free
    for (int i = 0; i < 2; ++i) {
        if (!clients[i] || !clients[i].connected()) {
            WiFiClient newClient = tcpServer.available();
            if (newClient) {
                clients[i] = newClient;
                Serial.printf("Client %d connected\n", i + 1);
            }
        }
    }

    // Handle each connected client
    for (int i = 0; i < 2; ++i) {
        if (clients[i] && clients[i].connected()) {
            WiFiClient &client = clients[i];

            // ---- Read 4-byte image length ----
            if (client.available() < 4) continue; 
            uint8_t lenBuf[4];
            client.read(lenBuf, 4);
            uint32_t imgLength = lenBuf[0] | (lenBuf[1] << 8) | (lenBuf[2] << 16) | (lenBuf[3] << 24);

            // ---- Allocate buffer for full image ----
            uint8_t* imageBuf = (uint8_t*)malloc(imgLength);
            if (!imageBuf) {
                Serial.println("OOM allocating TCP image buffer");
                continue;
            }

            // ---- Read full image ----
            uint32_t received = 0;
            while (received < imgLength) {
                if (client.available() > 0) {
                    received += client.read(imageBuf + received, imgLength - received);
                }
            }

            // ---- Forward image to Serial using proper buffer ----
            tryFinalizeImageTCP(imageBuf, imgLength, i);

            free(imageBuf); // Free memory
        }
    }
}

} // namespace Comms
