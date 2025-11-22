#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// Sensor data from Sender 1 (Camera 1)
struct SensorPacket1 {
    bool sensor1, sensor2, sensor3;
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
    float currentHingeAngle;  // ⭐ NEW: Current hinge angle from sender
};

// Sensor data from Sender 2 (Camera 2)
struct SensorPacket2 {
    bool sensor4, sensor5, sensor6;
    float currentHingeAngle;  // ⭐ NEW: Current hinge angle from sender
};

// ⭐ NEW: Position command from Jetson
struct PositionCommand {
    float linear_x;   // Forward/backward
    float linear_y;   // Left/right (strafe)
    float linear_z;   // Up/down
    float angular_x;  // Roll
    float angular_y;  // Pitch
    float angular_z;  // Yaw (turn)
};

// ⭐ NEW: Hinge command from Jetson
struct HingeCommand {
    uint8_t hingeID;     // Which hinge (1 or 2)
    float targetAngle;   // Desired angle in degrees
};

extern SensorPacket1 packet1;
extern SensorPacket2 packet2;
extern PositionCommand positionCmd;
extern HingeCommand hingeCmd;

#endif