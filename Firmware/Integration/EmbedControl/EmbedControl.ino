#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
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
void resetI2CBus() {
  pinMode(21, OUTPUT);  // SDA
  pinMode(22, OUTPUT);  // SCL
  
  // Generate 9 clock pulses to reset any stuck slave
  for (int i = 0; i < 9; i++) {
    digitalWrite(22, HIGH);
    delayMicroseconds(5);
    digitalWrite(22, LOW);
    delayMicroseconds(5);
  }
  
  // Generate STOP condition
  digitalWrite(21, LOW);
  delayMicroseconds(5);
  digitalWrite(22, HIGH);
  delayMicroseconds(5);
  digitalWrite(21, HIGH);
  delayMicroseconds(5);
  
  // Return pins to I2C control
  pinMode(21, INPUT);
  pinMode(22, INPUT);
  delay(10);
}
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Disable WiFi during I2C initialization
  WiFi.mode(WIFI_OFF);


  // Motor setup
  setupMotor(hinge1);
  // setupMotor(L12);
  // setupMotor(R56);
  // setupMotor(L34);
  // setupMotor(R78);

  pinMode(EN_PIN, INPUT_PULLDOWN);
  pinMode(BUT_PIN, INPUT_PULLDOWN);

  // PID and Encoder
  myPidSetup(&hingePID);
  PIDController_Init(&hingePID);
  initEncoder();

  // Initialize I2C with slower speed BEFORE any I2C devices
  resetI2CBus();
  Wire.begin();
  Wire.setClock(100000);  // 100kHz instead of default 400kHz
  delay(100);

  // Initialize I2C devices in order
  // initMPU();   // Initialize distance sensors first
  delay(200);
  // initVL53L0X();       // Then initialize MPU6050
  delay(200);

  // Non-I2C devices
  initCamera();
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
    // readVL53L0X();
    // readMPU();
    lastSensorRead = now;
  }
  
  // ========== LOW PRIORITY: SENSOR DATA TRANSMISSION (runs every 100ms) ==========
  if (now - lastSensorSend >= 100) {
    // sendSensorData();
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