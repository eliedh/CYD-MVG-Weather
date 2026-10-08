// Open-Meteo forecast parser (https://open-meteo.com, no API key).
#pragma once

#include <string>

#include "byte_source.h"
#include "model.h"

namespace core {

// Builds the request URL for the given location (timeformat=unixtime,
// timezone=Europe/Berlin, current + hourly + daily blocks).
std::string openMeteoUrl(float lat, float lon);

// Parses a forecast response. `now` (epoch s) selects the hourly window.
bool parseOpenMeteo(ByteSource& in, int64_t now, WeatherData& out);

}  // namespace core
