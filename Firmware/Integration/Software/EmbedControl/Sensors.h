#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <time.h>
#include <sys/time.h>

// ---------- Pins & I2C Addresses ----------
#define XSHUT1 12               // Shutdown pin for sensor 1
#define XSHUT2 13               // Shutdown pin for sensor 2
#define XSHUT3 15                // Shutdown pin for sensor 3
// #define XSHUT4 4                // Shutdown pin for sensor 4

#define LOX1_ADDRESS 0x44       // I2C address for sensor 1
#define LOX2_ADDRESS 0x45       // I2C address for sensor 2
#define LOX3_ADDRESS 0x46       // I2C address for sensor 3
// #define LOX4_ADDRESS 0x47       // I2C address for sensor 4

// ---------- Sensor Data Structure ----------
// Sensor data to send to receiver
struct SensorPacket {
    bool sensor1, sensor2, sensor3;
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
    // float currentHingeAngle;  // ⭐ NEW: Current hinge angle
};

// ⭐ NEW: Position command from Jetson (via receiver)
struct PositionCommand {
    float linear_x;   // Forward/backward
    float linear_y;   // Left/right (strafe)
    float linear_z;   // Up/down
    float angular_x;  // Roll
    float angular_y;  // Pitch
    float angular_z;  // Yaw (turn)
};

// ⭐ NEW: Hinge command from Jetson (via receiver)
struct HingeCommand {
    uint8_t hingeID;     // Which hinge (1 or 2)
    float targetAngle;   // Desired angle in degrees
};

extern TwoWire I2C_SENSORS;
extern Adafruit_VL53L0X lox1, lox2, lox3;
extern Adafruit_MPU6050 mpu;
extern SensorPacket sensorData;
extern PositionCommand positionCmd;
extern HingeCommand hingeCmd;

// ---------- Sensor Functions ----------
void initVL53L0X();        // Initialize 4 VL53L0X sensors with unique addresses
void initMPU();            // Initialize MPU6050 accelerometer/gyroscope
void readVL53L0X();        // Read distance data from 4 VL53L0X sensors
void readMPU();            // Read accelerometer and gyroscope data from MPU6050

#endif  // SENSORS_H