#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Adafruit_VL53L0X.h>

#define XSHUT1 12
#define XSHUT2 13
#define XSHUT3 2
#define XSHUT4 4

#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32
#define LOX4_ADDRESS 0x33

typedef struct {
    bool sensor5;
    bool sensor6;
    bool sensor7;
    bool sensor8;
} SensorPacket;

extern SensorPacket sensorData;
extern Adafruit_VL53L0X lox1, lox2, lox3, lox4;

void initVL53L0X();
void readVL53L0X();
void sendSensorData();

#endif
