#ifndef RELIABILTY_TEST_H
#define RELIABILTY_TEST_H

#include <Arduino.h>

namespace ReliabilityTest {
    // Test configuration
    extern const int TOTAL_TEST_IMAGES;
    extern int imagesSent;
    extern int imagesSuccess;
    extern int imagesFailed;
    extern bool testRunning;
    
    void runCameraReliabilityTest();
    void printCameraResults();
}

#endif