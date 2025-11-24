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
const int IN_1A = 33, IN_2A = 14;
const int IN_1B = 32, IN_2B = 27;
const int IN_3B = 2, IN_4B = 4;
// const int IN_1C = 35, IN_2C = 34;
// const int IN_3C = 15, IN_4C = 2;
const int POT_PIN = 34;
const int EN_PIN = 12;
const int BUT_PIN = 19;

// ============================================================================
// OBJECTS
// ============================================================================
Motor hinge1(IN_1A, IN_2A);
Motor L12(IN_1B, IN_2B);
Motor R56(IN_3B, IN_4B);
// Motor L34(IN_3C, IN_4C);
// Motor R78(IN_1C, IN_2C);

PIDController hingePID;

// ============================================================================
// GLOBALS
// ============================================================================
volatile bool triggerReceived = false;
volatile bool positionCmdReceived = false;  // ⭐ NEW
volatile bool hingeCmdReceived = false;     // ⭐ NEW
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
void handlePositionCommand();   // ⭐ NEW
void handleHingeCommand();      // ⭐ NEW
void executeMotorCommands(float linearX, float linearY, float angularZ);  // ⭐ NEW


// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  setupMotor(hinge1);
  setupMotor(L12);
  setupMotor(R56);
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
  initMPU();      // Uses I2C_SENSORS internally
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

    // sensorData.currentHingeAngle = curAngle;  // ⭐ NEW

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

    // Serial.print("Current: "); Serial.println(curAngle);
    // Serial.print("Set: "); Serial.println(setAngle);
    // Serial.print("Output: "); Serial.println(output);
    
    lastPIDUpdate = now;

    // turnLeft(256);
  }
  
  // ========== MEDIUM PRIORITY: SENSOR READING (runs every 50ms) ==========
  if (now - lastSensorRead >= 50) {
    readVL53L0X();
    readMPU();
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
// ⭐ NEW: ESP-NOW CALLBACK - Enhanced to handle multiple packet types
// ============================================================================
void onTriggerReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == 0) return;
  
  // Check first byte for packet type identifier
  uint8_t packetType = data[0];
  
  if (len == 1 && packetType == 1) {
    // ========== TRIGGER MESSAGE ==========
    triggerReceived = true;
  }
  else if (packetType == 0xAA && len == sizeof(PositionCommand) + 1) {
    // ========== POSITION COMMAND ==========
    memcpy(&positionCmd, &data[1], sizeof(PositionCommand));
    positionCmdReceived = true;
    
    Serial.println("Position command received");
    Serial.printf("  Linear: X=%.2f Y=%.2f Z=%.2f\n", 
                  positionCmd.linear_x, positionCmd.linear_y, positionCmd.linear_z);
    Serial.printf("  Angular: X=%.2f Y=%.2f Z=%.2f\n", 
                  positionCmd.angular_x, positionCmd.angular_y, positionCmd.angular_z);
  }
  else if (packetType == 0xBB && len == sizeof(HingeCommand) + 1) {
    // ========== HINGE COMMAND ==========
    memcpy(&hingeCmd, &data[1], sizeof(HingeCommand));
    hingeCmdReceived = true;
    
    Serial.printf("Hinge command received: ID=%d, Angle=%.2f\n", 
                  hingeCmd.hingeID, hingeCmd.targetAngle);
  }
}

// ============================================================================
// ⭐ NEW: Handle Position Command
// ============================================================================
void handlePositionCommand() {
  // Extract velocity commands
  float linearX = positionCmd.linear_x;   // Forward/backward
  float linearY = positionCmd.linear_y;   // Strafe left/right
  float angularZ = positionCmd.angular_z; // Turn (yaw)
  
  // Execute motor commands based on position
  executeMotorCommands(linearX, linearY, angularZ);
  
  Serial.printf("Executing: FWD=%.2f, STRAFE=%.2f, TURN=%.2f\n", 
                linearX, linearY, angularZ);
}

// ============================================================================
// ⭐ NEW: Handle Hinge Command
// ============================================================================
void handleHingeCommand() {
  // Check if this command is for our hinge
  // Assuming CAMERA_ID determines hinge ID (1 or 2)
  if (hingeCmd.hingeID == CAMERA_ID) {
    // Update setpoint for PID controller
    setAngle = hingeCmd.targetAngle;
    
    Serial.printf("Setting hinge %d to %.2f degrees\n", 
                  hingeCmd.hingeID, hingeCmd.targetAngle);
  } else {
    Serial.printf("Hinge command for ID %d ignored (I am ID %d)\n", 
                  hingeCmd.hingeID, CAMERA_ID);
  }
}

// ============================================================================
// ⭐ NEW: Execute Motor Commands (Mecanum/Omnidirectional Drive)
// ============================================================================
void executeMotorCommands(float linearX, float linearY, float angularZ) {
  // Convert velocity commands to motor speeds
  // This is for a 4-wheel mecanum drive or similar
  
  // Scale factors (tune these based on your robot)
  const float LINEAR_SCALE = 200.0;   // Max speed for linear motion
  const float ANGULAR_SCALE = 150.0;  // Max speed for rotation
  
  // Calculate individual wheel speeds
  // For mecanum drive:
  // Front-left  = linearX - linearY - angularZ
  // Front-right = linearX + linearY + angularZ
  // Rear-left   = linearX + linearY - angularZ
  // Rear-right  = linearX - linearY + angularZ
  
  int speedL12 = (int)((linearX - linearY - angularZ) * LINEAR_SCALE);
  int speedR56 = (int)((linearX + linearY + angularZ) * LINEAR_SCALE);
  
  // Clamp speeds to valid PWM range
  speedL12 = constrain(speedL12, -255, 255);
  speedR56 = constrain(speedR56, -255, 255);
  
  // Apply motor commands
  if (speedL12 > 0) {
    hingeUp(speedL12, L12);
  } else if (speedL12 < 0) {
    hingeDown(abs(speedL12), L12);
  } else {
    stopMotor(L12);
  }
  
  if (speedR56 > 0) {
    hingeUp(speedR56, R56);
  } else if (speedR56 < 0) {
    hingeDown(abs(speedR56), R56);
  } else {
    stopMotor(R56);
  }
  
  Serial.printf("Motor speeds: L12=%d, R56=%d\n", speedL12, speedR56);
}

// ============================================================================
// MOTOR CONTROL FUNCTIONS
// ============================================================================
void turnLeft(int speed){
  // hingeUp(speed/2, L34);
  hingeDown(speed/2, L12);
  hingeUp(speed, R56);
  // hingeUp(speed, R78);
}

void hingeUp(int motorSpeed, Motor &motorName) {
  analogWrite(motorName.in1, motorSpeed);
  analogWrite(motorName.in2, 0);
}

void hingeDown(int motorSpeed, Motor &motorName) {
  analogWrite(motorName.in1, 0);
  analogWrite(motorName.in2, motorSpeed);
}

void stopRobot(){
  // stopMotor(L34);
  stopMotor(L12);
  stopMotor(R56);
  // stopMotor(R78);
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
  pid->Kp = 2.0f;
  pid->Ki = 0.0f;
  pid->Kd = 0.0f;
  pid->tau = 0.02f;
  pid->T = 0.01f;
  pid->limMin = -256.0f;
  pid->limMax = 256.0f;
  pid->limMinInt = -10.0f;
  pid->limMaxInt = 10.0f;
}