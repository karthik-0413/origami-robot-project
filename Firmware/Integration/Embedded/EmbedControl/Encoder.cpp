#include "esp32-hal-gpio.h"
#include "Encoder.h"

volatile int counter = 0;
volatile bool direction = true; // true = Clockwise

const int pulsesPerRevolution = 1050;

void initEncoder() {
  pinMode(A_Signal, INPUT);
  pinMode(B_Signal, INPUT);
  attachInterrupt(digitalPinToInterrupt(A_Signal), handleEncoder, CHANGE);

  Serial.print("Encoder Initialized\n");
}

void handleEncoder() {
  bool A = digitalRead(A_Signal);
  bool B = digitalRead(B_Signal);

  if (A == B) {
    counter++;
    direction = true;   // CW
  } else {
    counter--;
    direction = false;   //CCW
  }
}

float updateEncoder() {
  static int lastCounter = 0;

  noInterrupts();
  int localCount = counter;
  bool localDir = direction;
  interrupts();

  if (localCount != lastCounter) {
    //Serial.print("Encoder turned ");
    //Serial.print(localDir ? "Clockwise" : "Counter-Clockwise");
    //Serial.print(" | Count: ");
    //Serial.print(localCount);

    float angle = (float)localCount / pulsesPerRevolution * 360.0f;
    //Serial.print(" | Angle: ");
    //Serial.print(angle);
    //Serial.println("°");

    lastCounter = localCount;

    return angle;
  }

  //return last known angle if no change
  return (float)lastCounter / pulsesPerRevolution * 360.0f;
}
