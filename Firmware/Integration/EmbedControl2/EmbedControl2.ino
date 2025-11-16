#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>

#include "PID.h"
#include "Encoder.h"
#include "CameraModule.h"
#include "Sensors.h"
#include "Comms.h"

// ============================================================================
// MOTOR STRUCTURE
// ============================================================================
struct Motor {
  int in1, in2;
  Motor(int in1_, int in2_) : in1(in1_), in2(in2_) {}
};

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int IN_1A = 32, IN_2A = 27;
// const int IN_1B = 26, IN_2B = 25;
// const int IN_3B = 33, IN_4B = 4;
// const int IN_1C = 35, IN_2C = 34;
// const int IN_3C = 15, IN_4C = 2;
const int POT_PIN = 34;
const int EN_PIN = 12;
const int BUT_PIN = 19;

// ============================================================================
// OBJECTS
// ============================================================================
Motor hinge1(IN_1A, IN_2A);
// Motor L12(IN_1B, IN_2B);
// Motor R56(IN_3B, IN_4B);
// Motor L34(IN_3C, IN_4C);
// Motor R78(IN_1C, IN_2C);

PIDController hingePID;

// ============================================================================
// GLOBALS
// ============================================================================
volatile bool triggerReceived = false;
uint8_t ackMsg = 1;
float setAngle = 0.0;
float curAngle = 0.0;

// Timing variables to prevent blocking
unsigned long lastSensorRead = 0;
unsigned long lastSensorSend = 0;
unsigned long lastPIDUpdate = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void myPidSetup(PIDController*);
void hingeUp(int, Motor &);
void hingeDown(int, Motor &);
void stopMotor(Motor &);
void setupMotor(Motor &);
void onTriggerReceived(const esp_now_recv_info_t *, const uint8_t *, int);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  setupMotor(hinge1);
  pinMode(EN_PIN, INPUT_PULLDOWN);
  pinMode(BUT_PIN, INPUT_PULLDOWN);

  myPidSetup(&hingePID);
  PIDController_Init(&hingePID);
  initEncoder();

  // Initialize camera I2C (GPIO 21/22) FIRST
  Wire.begin(21, 22);
  Wire.setClock(100000);
  delay(200);

  initCamera();  // Camera uses default Wire on GPIO 21/22
  delay(500);

  // Initialize sensors on separate I2C (GPIO 25/26)
  initVL53L0X();  // Uses I2C_SENSORS internally
  delay(200);
  
  initWiFi();
  initESPNow();
  esp_now_register_recv_cb(onTriggerReceived);
  
  Serial.println("System initialized");
}

// ============================================================================
// MAIN LOOP - Time-sliced execution
// ============================================================================
void loop() {
  unsigned long now = millis();
  
  // ========== HIGH PRIORITY: PID CONTROL (runs every 1ms) ==========
  if (now - lastPIDUpdate >= 1) {
    curAngle = updateEncoder();
    int potValue = analogRead(POT_PIN);
    setAngle = (potValue - 2048) / 4096.0 * 360.0;
    
    int output = PIDController_Update(&hingePID, setAngle, curAngle);
    
    if (output > 0) {
      hingeUp(abs(output), hinge1);
    } else if (output < 0) {
      analogWrite(IN_1A, 0);
      analogWrite(IN_2A, abs(output));
    } else {
      stopMotor(hinge1);
    }
    
    lastPIDUpdate = now;
  }
  
  // ========== MEDIUM PRIORITY: SENSOR READING (runs every 50ms) ==========
  if (now - lastSensorRead >= 50) {
    readVL53L0X();
    lastSensorRead = now;
  }
  
  // ========== LOW PRIORITY: SENSOR DATA TRANSMISSION (runs every 100ms) ==========
  if (now - lastSensorSend >= 100) {
    sendSensorData();
    lastSensorSend = now;
  }
  
  // ========== EVENT-DRIVEN: CAMERA TRIGGER ==========
  if (triggerReceived) {
    triggerReceived = false;
    captureAndSend();
    
    esp_err_t res = esp_now_send(receiverMAC, &ackMsg, 1);
    if (res == ESP_OK) {
      Serial.println("ACK sent");
    }
    delay(50);
  }
}

// ============================================================================
// ESP-NOW CALLBACK
// ============================================================================
void onTriggerReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == 1 && data[0] == 1) {
    // Serial.print("Received");
    triggerReceived = true;
  }
}

// ============================================================================
// MOTOR CONTROL FUNCTIONS
// ============================================================================
void hingeUp(int motorSpeed, Motor &motorName) {
  analogWrite(motorName.in1, motorSpeed);
  analogWrite(motorName.in2, 0);
}

void hingeDown(int motorSpeed, Motor &motorName) {
  analogWrite(motorName.in1, 0);
  analogWrite(motorName.in2, motorSpeed);
}

void stopMotor(Motor &motorName) {
  analogWrite(motorName.in1, 0);
  analogWrite(motorName.in2, 0);
}

void setupMotor(Motor &m) {
  pinMode(m.in1, OUTPUT);
  pinMode(m.in2, OUTPUT);
  digitalWrite(m.in1, LOW);
  digitalWrite(m.in2, LOW);
}

// ============================================================================
// PID SETUP
// ============================================================================
void myPidSetup(PIDController *pid) {
  pid->Kp = 5.0f;
  pid->Ki = 0.50f;
  pid->Kd = 0.0f;
  pid->tau = 0.02f;
  pid->T = 0.01f;
  pid->limMin = -256.0f;
  pid->limMax = 256.0f;
  pid->limMinInt = -10.0f;
  pid->limMaxInt = 10.0f;
}