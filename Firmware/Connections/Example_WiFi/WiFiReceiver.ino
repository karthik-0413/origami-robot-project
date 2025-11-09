#include <WiFi.h>

const char* ssid = "GL-MT3000-8d3";
const char* password = "QYAXW83ASS";

// Static IP setup
// Static IP setup
IPAddress local_IP(192, 168, 8, 150);    // <-- Pick an unused IP in your router's range
IPAddress gateway(192, 168, 8, 1);       // <-- Your router’s IP (GL.iNet default)
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(3333);

void setup() {
  Serial.begin(115200);

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
  }

  WiFi.begin(ssid, password);
  Serial.println("Connecting to Wi-Fi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("Receiver IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String msg = client.readStringUntil('\n');
        Serial.print("Received: ");
        Serial.println(msg);
      }
    }
    client.stop();
  }
}
