#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ---------- Structs from Senders ----------
typedef struct {
  bool sensor1;
  bool sensor2;
  bool sensor3;
  bool sensor4;
  float accelX;
  float accelY;
  float accelZ;
  float gyroX;
  float gyroY;
  float gyroZ;
} SensorPacket1;

typedef struct {
  bool sensor5;
  bool sensor6;
  bool sensor7;
  bool sensor8;
} SensorPacket2;

// ---------- MAC Addresses for Each Sender ----------
uint8_t sender1MAC[] = {0x14, 0x33, 0x5C, 0x0A, 0x48, 0x2C}; // Green Breadboard
uint8_t sender2MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x23, 0x64}; // Black Breadboard

// ---------- Image Counters ----------
uint32_t imageCount1 = 0;
uint32_t imageCount2 = 0;

// ---------- Global Sensor Storage ----------
SensorPacket1 packet1;
SensorPacket2 packet2;

// ---------- Helper ----------
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
  for (int i = 0; i < 6; i++) {
    if (mac1[i] != mac2[i]) return false;
  }
  return true;
}

// ---------- ESP-NOW Callback ----------
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;

  // Case 1: Image Done Marker
  if (len == 8 && memcmp(data, "IMG_DONE", 8) == 0) {
    if (compareMAC(mac, sender1MAC)) {
      imageCount1++;
    } else if (compareMAC(mac, sender2MAC)) {
      imageCount2++;
    }
    return;
  }

  // Case 2: Sender 1 (IMU + IR sensors 1–4)
  if (compareMAC(mac, sender1MAC) && len == sizeof(SensorPacket1)) {
    memcpy(&packet1, data, sizeof(SensorPacket1));
    return;
  }

  // Case 3: Sender 2 (IR sensors 5–8)
  if (compareMAC(mac, sender2MAC) && len == sizeof(SensorPacket2)) {
    memcpy(&packet2, data, sizeof(SensorPacket2));
    return;
  }

  // Case 4: Ignore image bytes to save RAM
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1);
  }

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Receiver initialized and ready for multiple senders.");
}

// ---------- Loop ----------
void loop() {
  static unsigned long lastPlotTime = 0;
  static unsigned long lastTerminalTime = 0;
  unsigned long now = millis();
  bool plotIR = true;
  // bool plotIR = false;

  // ---------- Serial Plotter for 8 IR sensors ----------
  if (now - lastPlotTime >= 100) { // plot every 100ms
    if (plotIR) {
      Serial.print("S1:"); Serial.print(packet1.sensor1);
      Serial.print(" S2:"); Serial.print(packet1.sensor2);
      Serial.print(" S3:"); Serial.print(packet1.sensor3);
      Serial.print(" S4:"); Serial.print(packet1.sensor4);
      Serial.print(" S5:"); Serial.print(packet2.sensor5);
      Serial.print(" S6:"); Serial.print(packet2.sensor6);
      Serial.print(" S7:"); Serial.print(packet2.sensor7);
      Serial.print(" S8:"); Serial.println(packet2.sensor8);
    } else if (!plotIR) {
      Serial.print("AccX:"); Serial.print(packet1.accelX);
      Serial.print(" AccY:"); Serial.print(packet1.accelY);
      Serial.print(" AccZ:"); Serial.print(packet1.accelZ);
      Serial.print(" GryX:"); Serial.print(packet1.gyroX);
      Serial.print(" GryY:"); Serial.print(packet1.gyroY);
      Serial.print(" GryZ:"); Serial.print(packet1.gyroZ);
    }

    lastPlotTime = now;
  }

  // ---------- Serial Monitor for IMU and image counts ----------
  if (now - lastTerminalTime >= 1000) {
    Serial.print("Images received from Sender 1 in last second: ");
    Serial.println(imageCount1);

    Serial.print("Images received from Sender 2 in last second: ");
    Serial.println(imageCount2);

    // Serial.println("----- IMU Data from Sender 1 -----");
    // Serial.printf("Accel -> X: %.2f Y: %.2f Z: %.2f\n",
    //               packet1.accelX, packet1.accelY, packet1.accelZ);
    // Serial.printf("Gyro  -> X: %.2f Y: %.2f Z: %.2f\n",
    //               packet1.gyroX, packet1.gyroY, packet1.gyroZ);
    // Serial.println("---------------------------------");

    imageCount1 = 0;
    imageCount2 = 0;
    lastTerminalTime = now;
  }
}
