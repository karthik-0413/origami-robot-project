// Code gotten from https://randomnerdtutorials.com/esp-now-two-way-communication-esp32/

#include <esp_now.h>
#include <WiFi.h>

// Broadcast address (sends to all ESP-NOW devices listening)
uint8_t broadcastAddress[] = {0x14, 0x33, 0x5c, 0x0a, 0x48, 0x2c};  // MAC Address of ESP labeled "1"

// Define message structure
typedef struct struct_message {
  // Add variables in here as necessary for future
  int value;
} struct_message;

struct_message myData;

// Other ESP 32 Info
esp_now_peer_info_t peerInfo;

// Function called when Data is Sent
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Function called when Data is Received
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  struct_message incomingReadings;
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

  Serial.print("Received value: ");
  Serial.println(incomingReadings.value);
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set device as a Wi-Fi Component
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packets
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Register peer
  memcpy(peerInfo.peer_addr, receiverMACAdddres, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  static int counter = 0;

  // Set value to send
  myData.value = counter++;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMACAdddres, (uint8_t *) &myData, sizeof(myData));
   
  if (result == ESP_OK) {
    Serial.print("Sent value: ");
    Serial.println(myData.value);
  } else {
    Serial.println("Error sending the data");
  }

  delay(2000); // Send every 2 seconds
}