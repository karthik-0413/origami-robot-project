#include "Sensors.h"
#include <Wire.h>

// Create separate I2C bus for sensors on GPIO 25/26
TwoWire I2C_SENSORS = TwoWire(1);

Adafruit_VL53L0X lox1, lox2, lox3; //, lox4;
SensorPacket sensorData;
PositionCommand positionCmd = {0, 0, 0, 0, 0, 0};  // ⭐ NEW
HingeCommand hingeCmd = {0, 0};                     // ⭐ NEW

void initVL53L0X() {
  // Initialize sensor I2C bus on GPIO 25/26
  I2C_SENSORS.begin(25, 26);
  I2C_SENSORS.setClock(100000);
  delay(200);
  
  Serial.println("Initializing VL53L0X on I2C1 (GPIO 25/26)...");
  
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  // pinMode(XSHUT4, OUTPUT);

  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  // digitalWrite(XSHUT4, LOW);
  delay(50);

  digitalWrite(XSHUT1, HIGH); 
  delay(50);
  if (!lox1.begin(LOX1_ADDRESS, false, &I2C_SENSORS)) {
    Serial.println("VL53L0X #1 failed!");
  }

  digitalWrite(XSHUT2, HIGH); 
  delay(50);
  if (!lox2.begin(LOX2_ADDRESS, false, &I2C_SENSORS)) {
    Serial.println("VL53L0X #2 failed!");
  }

  digitalWrite(XSHUT3, HIGH); 
  delay(50);
  if (!lox3.begin(LOX3_ADDRESS, false, &I2C_SENSORS)) {
    Serial.println("VL53L0X #3 failed!");
  }

  // digitalWrite(XSHUT4, HIGH); 
  // delay(50);
  // if (!lox4.begin(LOX4_ADDRESS, false, &I2C_SENSORS)) {
  //   Serial.println("VL53L0X #4 failed!");
  // }
  
  Serial.println("VL53L0X initialization complete");
}

void readVL53L0X() {
  VL53L0X_RangingMeasurementData_t measure[4];

  lox1.rangingTest(&measure[0], false);
  lox2.rangingTest(&measure[1], false);
  lox3.rangingTest(&measure[2], false);
  // lox4.rangingTest(&measure[3], false);

  bool results[4] = {false, false, false, false};

  for (int i = 0; i < 4; i++) {
    if (measure[i].RangeStatus != 4) {
      results[i] = (measure[i].RangeMilliMeter <= 50);
    }
  }

  sensorData.sensor4 = results[0];
  sensorData.sensor5 = results[1];
  sensorData.sensor6 = results[2];
  // sensorData.sensor8 = results[3];

  Serial.print("IR4: "); Serial.println(sensorData.sensor4);
  Serial.print("IR5: "); Serial.println(sensorData.sensor5);
  Serial.print("IR6: "); Serial.println(sensorData.sensor6);
}