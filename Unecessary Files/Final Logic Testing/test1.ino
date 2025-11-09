#include <Wire.h>
#include <SPI.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduCAM.h>
#include "memorysaver.h"

// ----- CAMERA -----
#define OV2640_MINI_2MP
#define CAM_CS 5
ArduCAM myCAM(OV2640, CAM_CS);

// ----- IR SENSORS -----
Adafruit_VL53L0X lox1, lox2, lox3, lox4;
const int XSHUT1 = 12;
const int XSHUT2 = 13;
const int XSHUT3 = 14;
const int XSHUT4 = 27;
bool ir1_detected = false, ir2_detected = false, ir3_detected = false, ir4_detected = false;
const uint16_t IR_THRESHOLD_MM = 500; // 50 cm

// ----- MPU6050 -----
Adafruit_MPU6050 mpu;
float accelX, accelY, accelZ;
float gyroX, gyroY, gyroZ;
float temperature;
unsigned long imu_timestamp;

// ----- POTENTIOMETER -----
const int POT_PIN = 15;
int pot_value = 0;
bool pot_in_range = false;
const int POT_MIN = 400; // example desired range
const int POT_MAX = 600;

// ----- DC MOTORS (DRV8833) -----
struct Motor {
  int pinA;
  int pinB;
  bool enabled;
};
Motor motors[5] = {
  {36, 39, false}, // Motor 1
  {34, 35, false}, // Motor 2
  {32, 33, false}, // Motor 3
  {25, 26, false}, // Motor 4
  {14, 27, false}  // Motor 5
};

// ----- SETUP -----
void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin(18, 19, 23, CAM_CS); // SCK, MISO, MOSI, CS

  // --------- INIT MOTORS ---------
  for (int i = 0; i < 5; i++) {
    pinMode(motors[i].pinA, OUTPUT);
    pinMode(motors[i].pinB, OUTPUT);
    digitalWrite(motors[i].pinA, LOW);
    digitalWrite(motors[i].pinB, LOW);
  }

  // --------- INIT POTENTIOMETER ---------
  pinMode(POT_PIN, INPUT);

  // --------- INIT MPU6050 ---------
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  // --------- INIT IR SENSORS ---------
  pinMode(XSHUT1, OUTPUT); pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT); pinMode(XSHUT4, OUTPUT);

  // Turn all off
  digitalWrite(XSHUT1, LOW); digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW); digitalWrite(XSHUT4, LOW);
  delay(10);

  // Sequential initialization and assign unique addresses
  digitalWrite(XSHUT1, HIGH); delay(10); lox1.begin(); lox1.setAddress(0x30);
  digitalWrite(XSHUT2, HIGH); delay(10); lox2.begin(); lox2.setAddress(0x31);
  digitalWrite(XSHUT3, HIGH); delay(10); lox3.begin(); lox3.setAddress(0x32);
  digitalWrite(XSHUT4, HIGH); delay(10); lox4.begin(); lox4.setAddress(0x33);

  // --------- INIT CAMERA ---------
  pinMode(CAM_CS, OUTPUT); digitalWrite(CAM_CS, HIGH);
  myCAM.write_reg(0x07, 0x80); delay(100);
  myCAM.write_reg(0x07, 0x00); delay(100);
  uint8_t temp;
  while (1) {
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp == 0x55) break;
    Serial.println("SPI Error, retrying..."); delay(1000);
  }
  uint8_t vid, pid;
  while (1) {
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid == 0x26) && ((pid == 0x41) || (pid == 0x42))) break;
    Serial.println("Camera not found, retrying..."); delay(1000);
  }
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  delay(100);
  myCAM.clear_fifo_flag();
}

// ----- LOOP -----
void loop() {
  VL53L0X_RangingMeasurementData_t measure;

  // --------- READ IR SENSORS ---------
  lox1.rangingTest(&measure, false);
  ir1_detected = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= IR_THRESHOLD_MM);
  lox2.rangingTest(&measure, false);
  ir2_detected = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= IR_THRESHOLD_MM);
  lox3.rangingTest(&measure, false);
  ir3_detected = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= IR_THRESHOLD_MM);
  lox4.rangingTest(&measure, false);
  ir4_detected = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= IR_THRESHOLD_MM);

  // --------- READ MPU6050 ---------
  sensors_event_t a, g, temp_event;
  mpu.getEvent(&a, &g, &temp_event);
  accelX = a.acceleration.x; accelY = a.acceleration.y; accelZ = a.acceleration.z;
  gyroX = g.gyro.x; gyroY = g.gyro.y; gyroZ = g.gyro.z;
  temperature = temp_event.temperature;
  imu_timestamp = millis();

  // --------- READ POTENTIOMETER ---------
  pot_value = analogRead(POT_PIN);
  pot_in_range = (pot_value >= POT_MIN && pot_value <= POT_MAX);

  // --------- UPDATE DC MOTORS ---------
  for (int i = 0; i < 5; i++) {
    if (motors[i].enabled) {
      digitalWrite(motors[i].pinA, HIGH);
      digitalWrite(motors[i].pinB, LOW);
    } else {
      digitalWrite(motors[i].pinA, LOW);
      digitalWrite(motors[i].pinB, LOW);
    }
  }

  // --------- CAPTURE CAMERA IMAGE ---------
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) delay(10);

  uint32_t length = myCAM.read_fifo_length();
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  while (length--) {
    SPI.transfer(0x00); // here you would send data via ESP-NOW
  }
  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  // --------- PRINT STATUS ---------
  Serial.print("IR1: "); Serial.print(ir1_detected);
  Serial.print(" IR2: "); Serial.print(ir2_detected);
  Serial.print(" IR3: "); Serial.print(ir3_detected);
  Serial.print(" IR4: "); Serial.println(ir4_detected);

  Serial.print("Accel: "); Serial.print(accelX); Serial.print(", "); Serial.print(accelY); Serial.print(", "); Serial.println(accelZ);
  Serial.print("Gyro: "); Serial.print(gyroX); Serial.print(", "); Serial.print(gyroY); Serial.print(", "); Serial.println(gyroZ);
  Serial.print("Pot: "); Serial.print(pot_value); Serial.print(" InRange: "); Serial.println(pot_in_range);

  delay(10); // loop delay
}
