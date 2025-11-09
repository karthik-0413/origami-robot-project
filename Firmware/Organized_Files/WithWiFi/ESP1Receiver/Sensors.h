#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// ---------- Sensor Data Structure ----------
typedef struct {
  bool sensor1, sensor2, sensor3, sensor4;   // Proximity flags for 4 VL53L0X sensors
  float accelX, accelY, accelZ;              // Accelerometer readings
  float gyroX, gyroY, gyroZ;                 // Gyroscope readings
} SensorPacket1;

typedef struct {
  bool sensor5, sensor6, sensor7, sensor8;   // Proximity flags for 4 VL53L0X sensors
} SensorPacket2;

// Global sensor data objects
extern SensorPacket1 packet1;
extern SensorPacket2 packet2;

#endif
