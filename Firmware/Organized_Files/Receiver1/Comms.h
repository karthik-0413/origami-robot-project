#ifndef COMMS_H
#define COMMS_H

#include <Arduino.h>
#include <esp_now.h>
#include "Sensors.h"
#include "ImageBuffer.h"

namespace Comms {
    void initESPNow();
    void registerCallback();
}

#endif
