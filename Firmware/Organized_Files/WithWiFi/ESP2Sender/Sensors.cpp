#include "Sensors.h"

// ---------- Sensor Objects ----------
Adafruit_VL53L0X lox1, lox2, lox3, lox4;      // Four VL53L0X distance sensors
SensorPacket sensorData;                      // Struct to hold all sensor readings


/****************************************************
 * Function: initVL53L0X
 * Description: Initializes four VL53L0X sensors with
 *              separate XSHUT pins and assigns unique I2C addresses.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Temporarily shuts down all sensors to avoid I2C conflicts.
 *    - Delays are used to allow sensors to power up properly.
 ****************************************************/
void initVL53L0X() {
  // Configure shutdown pins
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  pinMode(XSHUT3, OUTPUT);
  pinMode(XSHUT4, OUTPUT);

  // Turn all sensors off
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  digitalWrite(XSHUT3, LOW);
  digitalWrite(XSHUT4, LOW);
  delay(10);

  // Power on sensor 1
  digitalWrite(XSHUT1, HIGH); delay(10);
  // Initialize with unique I2C address
  lox1.begin(LOX1_ADDRESS);

  // Power on sensor 2
  digitalWrite(XSHUT2, HIGH); delay(10);
  // Initialize with unique I2C address
  lox2.begin(LOX2_ADDRESS);

  // Power on sensor 3
  digitalWrite(XSHUT3, HIGH); delay(10);
  // Initialize with unique I2C address
  lox3.begin(LOX3_ADDRESS);

  // Power on sensor 4
  digitalWrite(XSHUT4, HIGH); delay(10);
  // Initialize with unique I2C address
  lox4.begin(LOX4_ADDRESS);
}


/****************************************************
 * Function: readVL53L0X
 * Description: Reads distance measurements from all four
 *              VL53L0X sensors and updates boolean flags
 *              if an object is detected within 50mm.
 * Inputs: None
 * Outputs: Updates sensorData.sensor1/2/3/4
 * Notes:
 *    - RangeStatus == 4 indicates invalid measurement.
 *    - Loops through all sensors to check object proximity.
 ****************************************************/
void readVL53L0X() {
  VL53L0X_RangingMeasurementData_t measure[4];

  lox1.rangingTest(&measure[0], false);        // Read sensor 1
  lox2.rangingTest(&measure[1], false);        // Read sensor 2
  lox3.rangingTest(&measure[2], false);        // Read sensor 3
  lox4.rangingTest(&measure[3], false);        // Read sensor 4

  bool results[4] = {false, false, false, false};

  for (int i = 0; i < 4; i++) {
    if (measure[i].RangeStatus != 4) {         // Valid reading?
      results[i] = (measure[i].RangeMilliMeter <= 50); // Object within 50mm?
    }
  }

  // Store results
  sensorData.sensor5 = results[0];
  sensorData.sensor6 = results[1];
  sensorData.sensor7 = results[2];
  sensorData.sensor8 = results[3];
}