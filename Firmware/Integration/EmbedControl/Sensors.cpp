#include "Sensors.h"

// ---------- Sensor Objects ----------
Adafruit_VL53L0X lox1, lox2, lox3, lox4;      // Four VL53L0X distance sensors
Adafruit_MPU6050 mpu;                         // MPU6050 accelerometer/gyroscope
SensorPacket sensorData;                      // Struct to hold all sensor readings
unsigned long lastMPUTime = 0;                // Timer for reading MPU at intervals

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
  digitalWrite(XSHUT1, HIGH); 
  delay(10);
  lox1.begin(LOX1_ADDRESS);

  // Power on sensor 2
  digitalWrite(XSHUT2, HIGH); 
  delay(10);
  lox2.begin(LOX2_ADDRESS);

  // Power on sensor 3
  digitalWrite(XSHUT3, HIGH); 
  delay(10);
  lox3.begin(LOX3_ADDRESS);

  // Power on sensor 4
  digitalWrite(XSHUT4, HIGH); 
  delay(10);
  lox4.begin(LOX4_ADDRESS);
}

/****************************************************
 * Function: initMPU
 * Description: Initializes the MPU6050 sensor for 
 *              accelerometer and gyroscope measurements.
 * Inputs: None
 * Outputs: None
 * Notes:
 *    - Configures accelerometer and gyroscope ranges.
 *    - Sets the digital filter bandwidth for smoothing readings.
 ****************************************************/
void initMPU() {
  if (!mpu.begin()) {
    Serial.println("Failed to initialize MPU6050!");
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G); // ±8G accelerometer range
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);      // ±500°/s gyro range
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);   // 21 Hz filter bandwidth
}

/****************************************************
 * Function: readMPU
 * Description: Reads accelerometer and gyroscope data 
 *              from MPU6050 at ~50ms intervals.
 * Inputs: None
 * Outputs: Updates sensorData.accelX/Y/Z and gyroX/Y/Z
 * Notes:
 *    - Uses millis() to control sampling rate (~20Hz)
 ****************************************************/
void readMPU() {
  if (millis() - lastMPUTime >= 50) {          // Read every 50ms
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);               // Get sensor events

    sensorData.accelX = a.acceleration.x;      // Store accelerometer readings
    sensorData.accelY = a.acceleration.y;
    sensorData.accelZ = a.acceleration.z;
    sensorData.gyroX  = g.gyro.x;              // Store gyroscope readings
    sensorData.gyroY  = g.gyro.y;
    sensorData.gyroZ  = g.gyro.z;

    // Serial.print("AccX: "); Serial.println(sensorData.accelX);
    // Serial.print("AccY: "); Serial.println(sensorData.accelY);
    // Serial.print("AccZ: "); Serial.println(sensorData.accelZ);
    // Serial.print("GryX: "); Serial.println(sensorData.gyroX);
    // Serial.print("GryY: "); Serial.println(sensorData.gyroY);
    // Serial.print("GryZ: "); Serial.println(sensorData.gyroZ);

    lastMPUTime = millis();                    // Update last read time
  }
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
  sensorData.sensor1 = results[0];
  sensorData.sensor2 = results[1];
  sensorData.sensor3 = results[2];
  sensorData.sensor4 = results[3];

  Serial.print("IR1: "); Serial.println(sensorData.sensor1);
  Serial.print("IR2: "); Serial.println(sensorData.sensor2);
  Serial.print("IR3: "); Serial.println(sensorData.sensor3);
  Serial.print("IR4: "); Serial.println(sensorData.sensor4);
}