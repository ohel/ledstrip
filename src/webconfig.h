#pragma once

#include <Arduino.h>

namespace WebConfig {
    void setupWifi(void (*operate)(String, byte, byte, byte));
    void handleClient();
}
