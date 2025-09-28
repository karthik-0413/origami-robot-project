#include "Adafruit_VL53L0X.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

// ---------- VL53L0X (ToF) ----------
Adafruit_VL53L0X lox1 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox2 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox3 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox4 = Adafruit_VL53L0X();

// XSHUT pins
#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4

// I2C addresses
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

// ---------- IMU (MPU6050) ----------
Adafruit_MPU6050 mpu;

// ---------- ArduCAM ----------
#define CS 5
ArduCAM myCAM(OV2640, CS);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // ---- ArduCAM setup ----
  SPI.begin(18, 19, 23, CS); // SCK, MISO, MOSI, CS
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  // Reset CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);

  // SPI interface test
  uint8_t temp;
  while (1) {
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) {
      Serial.println("ArduCAM SPI Error!");
      delay(1000);
      continue;
    }
    Serial.println("ArduCAM SPI OK");
    break;
  }

  // Detect OV2640
  uint8_t vid, pid;
  while (1) {
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    if ((vid != 0x26) || ((pid != 0x41) && (pid != 0x42))) {
      Serial.println("OV2640 not found!");
      delay(1000);
      continue;
    }
    Serial.println("OV2640 detected!");
    break;
  }

  // ---- VL53L0X setup ----
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
  delay(10);

  initVL53L0X(lox1, XSHUT1, LOX1_ADDRESS, "VL53L0X #1");
  initVL53L0X(lox2, XSHUT2, LOX2_ADDRESS, "VL53L0X #2");
  initVL53L0X(lox3, XSHUT3, LOX3_ADDRESS, "VL53L0X #3");
  initVL53L0X(lox4, XSHUT4, LOX4_ADDRESS, "VL53L0X #4");

  // ---- IMU setup ----
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip!");
    while (1);
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  delay(100);
  myCAM.clear_fifo_flag();
}

void loop() {
  // ---- VL53L0X readings ----
  Serial.println("----- IR Sensors -----");
  readVL53L0X("Sensor 1", lox1);
  readVL53L0X("Sensor 2", lox2);
  readVL53L0X("Sensor 3", lox3);
  readVL53L0X("Sensor 4", lox4);

  // ---- IMU readings ----
  Serial.println("----- IMU Data -----");
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.printf("Accel X: %.2f Y: %.2f Z: %.2f m/s^2\n", a.acceleration.x, a.acceleration.y, a.acceleration.z);
  Serial.printf("Gyro  X: %.2f Y: %.2f Z: %.2f rad/s\n", g.gyro.x, g.gyro.y, g.gyro.z);

  // ---- ArduCAM capture ----
  Serial.println("----- Camera -----");
  captureImage();

  Serial.println("--------------------\n");
  delay(100);
}

// ---------- Functions ----------
void initVL53L0X(Adafruit_VL53L0X &lox, int xshutPin, int address, const char *label) {
  digitalWrite(xshutPin, HIGH);
  delay(10);
  if (!lox.begin(address)) {
    Serial.print("Failed to boot ");
    Serial.println(label);
    while (1);
  }
  Serial.print(label);
  Serial.print(" ready at 0x");
  Serial.println(address, HEX);
}

void readVL53L0X(const char *label, Adafruit_VL53L0X &lox) {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

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

void captureImage() {
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    delay(10);
  }

  uint32_t length = myCAM.read_fifo_length();
  Serial.print("Captured Image Length: ");
  Serial.println(length);

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  while (length--) {
    SPI.transfer(0x00); // For demo, not sending over Serial
  }
  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  Serial.println("IMG_DONE");
}
