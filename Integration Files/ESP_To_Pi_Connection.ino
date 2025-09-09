// ESP32 <-> Orange Pi 5 UART Communication
#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);   // Debugging over USB
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // UART2 for Orange Pi
}

void loop() {
  // Send data to Orange Pi
  static int counter = 0;
  Serial2.print("ESP32 says hello ");
  Serial2.println(counter);
  counter++;
  delay(1000);

  // Check if data came from Orange Pi
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    Serial.print("Received from Orange Pi: ");
    Serial.println(msg);
  }
}
