#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Pins & addresses
#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

// Struct for sending sensor + IMU data
typedef struct {
  bool sensor1, sensor2, sensor3, sensor4;
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
} SensorPacket;

extern SensorPacket sensorData;

// Functions
void initVL53L0X();
void initMPU();
void readVL53L0X();
void readMPU();

#endif
