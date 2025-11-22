#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

// Encoder pin definitions
#define A_Signal 36
#define B_Signal 39

// Shared encoder state
extern volatile int counter;
extern volatile bool direction;

// Function declarations
void initEncoder();
void handleEncoder();   // ISR
float updateEncoder();   // Call in loop()

#endif
