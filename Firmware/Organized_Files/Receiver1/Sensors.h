#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// ---------- Sensor structs ----------
typedef struct {
    bool sensor1;
    bool sensor2;
    bool sensor3;
    bool sensor4;
    float accelX;
    float accelY;
    float accelZ;
    float gyroX;
    float gyroY;
    float gyroZ;
} SensorPacket1;

typedef struct {
    bool sensor5;
    bool sensor6;
    bool sensor7;
    bool sensor8;
} SensorPacket2;

extern SensorPacket1 packet1;
extern SensorPacket2 packet2;

#endif
