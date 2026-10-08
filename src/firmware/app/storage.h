// NVS persistence (Preferences). Settings are one JSON document; Wi-Fi
// credentials are stored separately so they never appear in the settings API.
#pragma once

#include <string>

#include "settings.h"

namespace storage {

bool loadSettings(core::Settings& out);
bool saveSettings(const core::Settings& s);

bool loadWifi(std::string& ssid, std::string& pass);
bool saveWifi(const std::string& ssid, const std::string& pass);
void clearWifi();

// Erases everything this firmware stored (settings, Wi-Fi, calibration).
void factoryReset();

}  // namespace storage
