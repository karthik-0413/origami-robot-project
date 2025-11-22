#include "ReliabiltyTest.h"
#include "CameraModule.h"
#include "Comms.h"

namespace ReliabilityTest {

const int TOTAL_TEST_IMAGES = 100;
int imagesSent = 0;
int imagesSuccess = 0;
int imagesFailed = 0;
bool testRunning = false;

void runCameraReliabilityTest() {
    Serial.println("\n========================================");
    Serial.println("  CAMERA RELIABILITY TEST");
    Serial.println("========================================");
    Serial.println("Will capture and send 100 images");
    Serial.println("Tracking transmission success rate");
    Serial.println("\nStarting in 5 seconds...\n");
    delay(5000);
    
    testRunning = true;
    imagesSent = 0;
    imagesSuccess = 0;
    imagesFailed = 0;
    
    Serial.println("Test started!");
    
    // Run test loop
    while (imagesSent < TOTAL_TEST_IMAGES) {
        // Capture image
        myCAM.flush_fifo();
        myCAM.clear_fifo_flag();
        myCAM.start_capture();
        
        unsigned long timeout = millis();
        while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
            if (millis() - timeout > 5000) {
                Serial.println("❌ Camera capture timeout!");
                myCAM.clear_fifo_flag();
                imagesFailed++;
                imagesSent++;
                continue;
            }
        }
        
        uint32_t length = myCAM.read_fifo_length();
        
        if (length == 0 || length > 100000) {
            Serial.println("❌ Invalid image length");
            myCAM.clear_fifo_flag();
            imagesFailed++;
            imagesSent++;
            continue;
        }
        
        uint8_t* buffer = (uint8_t*)malloc(length);
        if (!buffer) {
            Serial.println("❌ Memory allocation failed");
            myCAM.clear_fifo_flag();
            imagesFailed++;
            imagesSent++;
            continue;
        }
        
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
        SPI.transferBytes(NULL, buffer, length);
        myCAM.CS_HIGH();
        myCAM.clear_fifo_flag();
        
        // Send via UDP
        bool success = sendImageUDP(buffer, length);
        free(buffer);
        
        imagesSent++;
        
        if (success) {
            imagesSuccess++;
            Serial.print("✓ ");
        } else {
            imagesFailed++;
            Serial.print("✗ ");
        }
        
        Serial.print("Image ");
        Serial.print(imagesSent);
        Serial.print("/");
        Serial.print(TOTAL_TEST_IMAGES);
        
        if (success) {
            Serial.println(" - SUCCESS");
        } else {
            Serial.println(" - FAILED");
        }
        
        // Print progress every 10 images
        if (imagesSent % 10 == 0) {
            float successRate = (imagesSuccess / (float)imagesSent) * 100.0;
            Serial.print("   Progress: ");
            Serial.print(successRate, 1);
            Serial.println("% success rate");
        }
        
        delay(50); // 500ms between captures
    }
    
    testRunning = false;
    printCameraResults();
}

void printCameraResults() {
    Serial.println("\n========================================");
    Serial.println("          TEST COMPLETE");
    Serial.println("========================================");
    
    Serial.print("Total images sent:    ");
    Serial.println(imagesSent);
    Serial.print("Successful:           ");
    Serial.println(imagesSuccess);
    Serial.print("Failed:               ");
    Serial.println(imagesFailed);
    
    float successRate = (imagesSuccess / (float)imagesSent) * 100.0;
    float failureRate = 100.0 - successRate;
    
    Serial.print("\nSuccess Rate:         ");
    Serial.print(successRate, 2);
    Serial.println("%");
    Serial.print("Failure Rate:         ");
    Serial.print(failureRate, 2);
    Serial.println("%");
    
    Serial.println("\n--- CSV FORMAT (copy to Excel) ---");
    Serial.print(imagesSent);
    Serial.print(",");
    Serial.print(imagesSuccess);
    Serial.print(",");
    Serial.print(imagesFailed);
    Serial.print(",");
    Serial.println(successRate, 2);
    
    if (successRate >= 95.0) {
        Serial.println("\n✅ PASSED (≥95% success rate)");
    } else {
        Serial.println("\n❌ FAILED (<95% success rate)");
    }
}

} // namespace ReliabilityTest