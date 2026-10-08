// Abfahrt – MVG/MVV departures and weather on the ESP32 Cheap Yellow Display.
//
// Core 1 (Arduino loop): UI – app::loop() renders and handles input.
// Core 0 (net task):     Wi-Fi, captive portal, web server API, HTTPS fetching.
#include <Arduino.h>

#include "app/app.h"

void setup() { app::setup(); }

void loop() { app::loop(); }
