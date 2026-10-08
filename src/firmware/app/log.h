// Always-on serial logging (independent of CORE_DEBUG_LEVEL), so heap, LDR
// readings and network events are visible in release builds at 115200 baud.
#pragma once

#include <Arduino.h>

#define LOGI(fmt, ...) Serial.printf("[%7.1f] " fmt "\n", millis() / 1000.0, ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[%7.1f] WARN " fmt "\n", millis() / 1000.0, ##__VA_ARGS__)
