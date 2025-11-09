#include <WiFi.h>

const char* ssid = "GL-MT3000-8d3";
const char* password = "QYAXW83ASS";
const char* receiverIP = "192.168.8.150";
const int receiverPort = 3333;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to Wi-Fi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("ESP IP: ");
  Serial.println(WiFi.localIP());

  if (client.connect(receiverIP, receiverPort)) {
    Serial.println("Connected to receiver!");
    client.println("Hello from sender!\n");
  } else {
    Serial.println("Connection failed.");
  }
}

void loop() {
  // Optional: send repeatedly
  if (client.connected()) {
    client.println("Ping from sender");
    Serial.println("Message sent!");
  }
  delay(2000);
}
