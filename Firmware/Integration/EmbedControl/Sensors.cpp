#include "Sensors.h"
#include <Wire.h>

// Create separate I2C bus for sensors on GPIO 25/26
TwoWire I2C_SENSORS = TwoWire(1);

Adafruit_VL53L0X lox1, lox2, lox3, lox4;
Adafruit_MPU6050 mpu;
SensorPacket sensorData;

void initVL53L0X() {
  // Initialize sensor I2C bus on GPIO 25/26
  I2C_SENSORS.begin(25, 26);
  I2C_SENSORS.setClock(100000);
  delay(200);
  
  Serial.println("Initializing VL53L0X on I2C1 (GPIO 25/26)...");
  
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);

  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
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

  digitalWrite(XSHUT4, HIGH); 
  delay(50);
  if (!lox4.begin(LOX4_ADDRESS, false, &I2C_SENSORS)) {
    Serial.println("VL53L0X #4 failed!");
  }
  
  Serial.println("VL53L0X initialization complete");
}

void initMPU() {
  Serial.println("Initializing MPU6050 on I2C1 (GPIO 25/26)...");
  
  // Software reset
  I2C_SENSORS.beginTransmission(0x68);
  I2C_SENSORS.write(0x6B);
  I2C_SENSORS.write(0x80);
  I2C_SENSORS.endTransmission();
  delay(100);
  
  // Wake up
  I2C_SENSORS.beginTransmission(0x68);
  I2C_SENSORS.write(0x6B);
  I2C_SENSORS.write(0x00);
  I2C_SENSORS.endTransmission();
  delay(100);
  
  if (!mpu.begin(0x68, &I2C_SENSORS)) {
    Serial.println("MPU6050 init failed!");
    return;
  }
  
  Serial.println("MPU6050 initialized successfully!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void readMPU() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  sensorData.accelX = a.acceleration.x;
  sensorData.accelY = a.acceleration.y;
  sensorData.accelZ = a.acceleration.z;
  sensorData.gyroX = g.gyro.x;
  sensorData.gyroY = g.gyro.y;
  sensorData.gyroZ = g.gyro.z;
}

void readVL53L0X() {
  VL53L0X_RangingMeasurementData_t measure[4];

  lox1.rangingTest(&measure[0], false);
  lox2.rangingTest(&measure[1], false);
  lox3.rangingTest(&measure[2], false);
  lox4.rangingTest(&measure[3], false);

  bool results[4] = {false, false, false, false};

  for (int i = 0; i < 4; i++) {
    if (measure[i].RangeStatus != 4) {
      results[i] = (measure[i].RangeMilliMeter <= 50);
    }
  }

  sensorData.sensor1 = results[0];
  sensorData.sensor2 = results[1];
  sensorData.sensor3 = results[2];
  sensorData.sensor4 = results[3];
}