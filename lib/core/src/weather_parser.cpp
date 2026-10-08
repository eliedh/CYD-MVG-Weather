#include "weather_parser.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>

namespace core {

WeatherIcon weatherIconForCode(uint8_t c) {
  switch (c) {
    case 0: return WeatherIcon::Clear;
    case 1: return WeatherIcon::MostlyClear;
    case 2: return WeatherIcon::PartlyCloudy;
    case 3: return WeatherIcon::Overcast;
    case 45:
    case 48: return WeatherIcon::Fog;
    case 51: case 53: case 55: case 56: case 57: return WeatherIcon::Drizzle;
    case 61: case 63: case 65: case 66: case 67: return WeatherIcon::Rain;
    case 80: case 81: case 82: return WeatherIcon::Showers;
    case 71: case 73: case 75: case 77: case 85: case 86: return WeatherIcon::Snow;
    case 95: case 96: case 99: return WeatherIcon::Thunder;
    default: return WeatherIcon::Unknown;
  }
}

std::string openMeteoUrl(float lat, float lon) {
  char buf[420];
  snprintf(buf, sizeof(buf),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,weather_code,is_day,wind_speed_10m"
           "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
           "&timezone=Europe%%2FBerlin&timeformat=unixtime&forecast_days=2",
           lat, lon);
  return buf;
}

static float f(JsonVariantConst v, float def = NAN) {
  return v.is<float>() ? v.as<float>() : def;  // null -> def
}

bool parseOpenMeteo(ByteSource& in, int64_t now, WeatherData& out) {
  JsonDocument filter;
  filter["current"] = true;
  for (const char* k : {"time", "temperature_2m", "precipitation_probability", "weather_code", "is_day"})
    filter["hourly"][k] = true;
  for (const char* k : {"time", "temperature_2m_max", "temperature_2m_min",
                        "precipitation_probability_max", "sunrise", "sunset"})
    filter["daily"][k] = true;

  JsonDocument doc;
  if (deserializeJson(doc, in, DeserializationOption::Filter(filter))) return false;

  JsonObjectConst cur = doc["current"];
  if (cur.isNull()) return false;
  float t = f(cur["temperature_2m"]);
  if (isnan(t)) return false;

  WeatherData w;
  w.valid = true;
  w.temp = t;
  w.feelsLike = f(cur["apparent_temperature"], t);
  w.code = cur["weather_code"] | 0;
  w.isDay = (cur["is_day"] | 1) != 0;
  w.windKmh = f(cur["wind_speed_10m"], 0);
  if (!now) now = cur["time"] | (int64_t)0;

  // Daily block: pick the entry for today's date (first entry whose day
  // contains `now`; fall back to index 0).
  JsonObjectConst daily = doc["daily"];
  JsonArrayConst dTime = daily["time"];
  size_t di = 0;
  for (size_t i = 0; i < dTime.size(); i++) {
    int64_t dt = dTime[i] | (int64_t)0;
    if (dt && dt <= now) di = i;
  }
  w.tMax = f(daily["temperature_2m_max"][di], t);
  w.tMin = f(daily["temperature_2m_min"][di], t);
  w.precipMaxToday = daily["precipitation_probability_max"][di] | 0;
  w.sunrise = daily["sunrise"][di] | (int64_t)0;
  w.sunset = daily["sunset"][di] | (int64_t)0;

  // Hourly: start at the hour containing `now`.
  JsonObjectConst hourly = doc["hourly"];
  JsonArrayConst hTime = hourly["time"];
  size_t start = 0;
  for (size_t i = 0; i < hTime.size(); i++) {
    int64_t ht = hTime[i] | (int64_t)0;
    if (ht <= now) start = i;
    else break;
  }
  uint8_t rainMax = 0;
  for (size_t i = start; i < hTime.size() && w.nHours < kMaxHours; i++) {
    HourForecast& h = w.hours[w.nHours++];
    h.time = hTime[i] | (int64_t)0;
    h.temp = f(hourly["temperature_2m"][i], t);
    h.precipProb = hourly["precipitation_probability"][i] | 0;
    h.code = hourly["weather_code"][i] | 0;
    h.isDay = (hourly["is_day"][i] | 1) != 0;
    if (i < start + 6 && h.precipProb > rainMax) rainMax = h.precipProb;
  }
  w.rainNextHours = rainMax;
  out = w;
  return true;
}

}  // namespace core
