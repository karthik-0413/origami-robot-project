#include "Adafruit_VL53L0X.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// Four VL53L0X sensor objects
Adafruit_VL53L0X lox1 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox2 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox3 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox4 = Adafruit_VL53L0X();

// IMU object
Adafruit_MPU6050 mpu;

// XSHUT pins for VL53L0X
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

  // ---- Setup XSHUT pins ----
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);

  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
  delay(10);

  // ---- Initialize VL53L0X sensors ----
  digitalWrite(XSHUT1, HIGH);
  delay(10);
  if (!lox1.begin(LOX1_ADDRESS)) {
    Serial.println(F("Failed to boot VL53L0X #1"));
    while (1);
  }

  digitalWrite(XSHUT2, HIGH);
  delay(10);
  if (!lox2.begin(LOX2_ADDRESS)) {
    Serial.println(F("Failed to boot VL53L0X #2"));
    while (1);
  }

  digitalWrite(XSHUT3, HIGH);
  delay(10);
  if (!lox3.begin(LOX3_ADDRESS)) {
    Serial.println(F("Failed to boot VL53L0X #3"));
    while (1);
  }

  digitalWrite(XSHUT4, HIGH);
  delay(10);
  if (!lox4.begin(LOX4_ADDRESS)) {
    Serial.println(F("Failed to boot VL53L0X #4"));
    while (1);
  }

  // ---- Initialize IMU ----
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(100);
}

void loop() {
  VL53L0X_RangingMeasurementData_t measure1, measure2, measure3, measure4;

  // ---- Read all VL53L0X sensors ----
  lox1.rangingTest(&measure1, false);
  lox2.rangingTest(&measure2, false);
  lox3.rangingTest(&measure3, false);
  lox4.rangingTest(&measure4, false);

  Serial.println("----- IR Sensors -----");
  printSensorResult("Sensor 1", measure1);
  printSensorResult("Sensor 2", measure2);
  printSensorResult("Sensor 3", measure3);
  printSensorResult("Sensor 4", measure4);

  // ---- Read IMU data ----
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  Serial.println("----- IMU Data -----");
  Serial.print("Accel X: "); Serial.print(a.acceleration.x); Serial.print(" m/s^2, ");
  Serial.print("Y: "); Serial.print(a.acceleration.y); Serial.print(" m/s^2, ");
  Serial.print("Z: "); Serial.println(a.acceleration.z);

  Serial.print("Gyro X: "); Serial.print(g.gyro.x); Serial.print(" rad/s, ");
  Serial.print("Y: "); Serial.print(g.gyro.y); Serial.print(" rad/s, ");
  Serial.print("Z: "); Serial.println(g.gyro.z);

  Serial.println("--------------------");

  delay(100);
}

// Helper function to print IR sensor result
void printSensorResult(const char* label, VL53L0X_RangingMeasurementData_t measure) {
  Serial.print(label);
  Serial.print(": ");
  if (measure.RangeStatus != 4) {
    if (measure.RangeMilliMeter <= 50) {
      Serial.println("DETECT");
    } else {
      Serial.println("clear");
    }
  } else {
    Serial.println("out of range");
  }
}
