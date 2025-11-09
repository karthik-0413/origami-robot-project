#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Adafruit_VL53L0X.h>

// ---------- Pins & I2C Addresses ----------
#define XSHUT1 12               // Shutdown pin for sensor 1
#define XSHUT2 13               // Shutdown pin for sensor 2
#define XSHUT3 2                // Shutdown pin for sensor 3
#define XSHUT4 4                // Shutdown pin for sensor 4

#define LOX1_ADDRESS 0x30       // I2C address for sensor 1
#define LOX2_ADDRESS 0x31       // I2C address for sensor 2
#define LOX3_ADDRESS 0x32       // I2C address for sensor 3
#define LOX4_ADDRESS 0x33       // I2C address for sensor 4

// ---------- Sensor Data Structure ----------
typedef struct {
  bool sensor5, sensor6, sensor7, sensor8;   // Proximity flags for 4 VL53L0X sensors
} SensorPacket;

extern SensorPacket sensorData;  // Global sensor data object

void initVL53L0X();        // Initialize 4 VL53L0X sensors with unique addresses
void readVL53L0X();        // Read distance data from 4 VL53L0X sensors

#endif  // SENSORS_H
