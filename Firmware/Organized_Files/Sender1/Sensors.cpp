#include "Sensors.h"

Adafruit_VL53L0X lox1, lox2, lox3, lox4;
Adafruit_MPU6050 mpu;
SensorPacket sensorData;
unsigned long lastMPUTime = 0;

void initVL53L0X() {
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);

  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
  delay(10);

  digitalWrite(XSHUT1, HIGH); delay(10);
  lox1.begin(LOX1_ADDRESS);

  digitalWrite(XSHUT2, HIGH); delay(10);
  lox2.begin(LOX2_ADDRESS);

  digitalWrite(XSHUT3, HIGH); delay(10);
  lox3.begin(LOX3_ADDRESS);

  digitalWrite(XSHUT4, HIGH); delay(10);
  lox4.begin(LOX4_ADDRESS);
}

void initMPU() {
  if (!mpu.begin()) while (1);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void readMPU() {
  if (millis() - lastMPUTime >= 50) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sensorData.accelX = a.acceleration.x;
    sensorData.accelY = a.acceleration.y;
    sensorData.accelZ = a.acceleration.z;
    sensorData.gyroX  = g.gyro.x;
    sensorData.gyroY  = g.gyro.y;
    sensorData.gyroZ  = g.gyro.z;
    lastMPUTime = millis();
  }
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
