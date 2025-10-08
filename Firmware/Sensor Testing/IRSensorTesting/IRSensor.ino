#include "Adafruit_VL53L0X.h"

// Two sensor objects
Adafruit_VL53L0X lox1 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox2 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox3 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox4 = Adafruit_VL53L0X();

// XSHUT pins (pick two free GPIOs on your ESP32)
#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4

// New I2C addresses for sensors
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);

  // Keep both sensors in reset
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
  delay(10);

  // Initialize first sensor
  digitalWrite(XSHUT1, HIGH);
  delay(10);
  if (!lox1.begin(LOX1_ADDRESS)) {
    Serial.println(F("Failed to boot first VL53L0X"));
    while (1);
  }
  Serial.println(F("First VL53L0X ready at 0x30"));

  // Initialize second sensor
  digitalWrite(XSHUT2, HIGH);
  delay(10);
  if (!lox2.begin(LOX2_ADDRESS)) {
    Serial.println(F("Failed to boot second VL53L0X"));
    while (1);
  }
  Serial.println(F("Second VL53L0X ready at 0x31"));

  // Initialize third sensor
  digitalWrite(XSHUT3, HIGH);
  delay(10);
  if (!lox3.begin(LOX3_ADDRESS)) {
    Serial.println(F("Failed to boot third VL53L0X"));
    while (1);
  }
  Serial.println(F("Third VL53L0X ready at 0x31"));

  // Initialize fourth sensor
  digitalWrite(XSHUT4, HIGH);
  delay(10);
  if (!lox4.begin(LOX4_ADDRESS)) {
    Serial.println(F("Failed to boot fourth VL53L0X"));
    while (1);
  }
  Serial.println(F("Fourth VL53L0X ready at 0x31"));
}

void loop() {
  VL53L0X_RangingMeasurementData_t measure1, measure2, measure3, measure4;

  // Read first sensor
  lox1.rangingTest(&measure1, false);
  if (measure1.RangeStatus != 4) {
    if (measure1.RangeMilliMeter <= 50) {
      Serial.println("Sensor 1: DETECT");
    } else {
      Serial.println("Sensor 1: clear");
    }
  } else {
    Serial.println("Sensor 1: out of range");
  }

  // Read second sensor
  lox2.rangingTest(&measure2, false);
  if (measure2.RangeStatus != 4) {
    if (measure2.RangeMilliMeter <= 50) {
      Serial.println("Sensor 2: DETECT"); 
    } else {
      Serial.println("Sensor 2: clear");
    }
  } else {
    Serial.println("Sensor 2: out of range");
  }

  // Read third sensor
  lox3.rangingTest(&measure3, false);
  if (measure3.RangeStatus != 4) {
    if (measure3.RangeMilliMeter <= 50) {
      Serial.println("Sensor 3: DETECT");
    } else {
      Serial.println("Sensor 3: clear");
    }
  } else {
    Serial.println("Sensor 3: out of range");
  }

  // Read second sensor
  lox4.rangingTest(&measure4, false);
  if (measure4.RangeStatus != 4) {
    if (measure4.RangeMilliMeter <= 50) {
      Serial.println("Sensor 4: DETECT");
    } else {
      Serial.println("Sensor 4: clear");
    }
  } else {
    Serial.println("Sensor 4: out of range");
  }

  delay(200);
}
