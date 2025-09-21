#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "NetworkName";
const char* password = "Password";

const char* laptopIP = "LaptopIPAddress";
const int laptopPort = 5005;

WiFiUDP udp;

int counter = 0;

void setup() {
  Serial.begin(115200);
  
  delay(500);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
}

void loop() {
  udp.beginPacket(laptopIP, laptopPort);
  udp.write((uint8_t*)&counter, sizeof(counter));
  udp.endPacket();

  Serial.println("Sent counter: " + String(counter));
  counter++;s
  delay(1000);
}
