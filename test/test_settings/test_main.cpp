#include <string.h>
#include <unity.h>

#include "settings.h"

using namespace core;

void setUp() {}
void tearDown() {}

void test_roundtrip() {
  Settings s;
  s.lang = Lang::En;
  StopConfig a;
  a.id = "de:09162:500";
  a.name = "Münchner Freiheit";
  a.label = "Freiheit";
  a.lat = 48.16153f;
  a.lon = 11.5863f;
  a.walkMin = 4;
  a.typeMask = typeBit(TransportType::UBahn) | typeBit(TransportType::Tram);
  a.excludes = {"N40", "U3>Fürstenried West"};
  s.stops.push_back(a);
  s.weatherOverride = true;
  s.weatherLat = 47.9f;
  s.weatherLon = 11.3f;
  s.weatherName = "Starnberg";
  s.brightness = 55;
  s.autoBrightness = false;
  s.nightMode = NightMode::Dark;
  s.nightStart = 23 * 60;
  s.nightEnd = 6 * 60;
  s.touchCalValid = true;
  for (int i = 0; i < 8; i++) s.touchCal[i] = (uint16_t)(100 * i + 1);

  std::string j = settingsToJson(s);
  Settings r;
  TEST_ASSERT_TRUE(settingsFromJson(j.c_str(), r));
  TEST_ASSERT_EQUAL((int)Lang::En, (int)r.lang);
  TEST_ASSERT_EQUAL(1, (int)r.stops.size());
  TEST_ASSERT_EQUAL_STRING("Münchner Freiheit", r.stops[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("Freiheit", r.stops[0].label.c_str());
  TEST_ASSERT_EQUAL(4, r.stops[0].walkMin);
  TEST_ASSERT_EQUAL(a.typeMask, r.stops[0].typeMask);
  TEST_ASSERT_EQUAL(2, (int)r.stops[0].excludes.size());
  TEST_ASSERT_EQUAL_STRING("U3>Fürstenried West", r.stops[0].excludes[1].c_str());
  TEST_ASSERT_TRUE(r.weatherOverride);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 47.9, r.weatherLat);
  TEST_ASSERT_EQUAL_STRING("Starnberg", r.weatherName.c_str());
  TEST_ASSERT_EQUAL(55, r.brightness);
  TEST_ASSERT_FALSE(r.autoBrightness);
  TEST_ASSERT_EQUAL((int)NightMode::Dark, (int)r.nightMode);
  TEST_ASSERT_EQUAL(23 * 60, r.nightStart);
  TEST_ASSERT_TRUE(r.touchCalValid);
  TEST_ASSERT_EQUAL(701, r.touchCal[7]);
}

void test_defaults_and_garbage() {
  Settings r;
  TEST_ASSERT_TRUE(settingsFromJson("{}", r));
  TEST_ASSERT_EQUAL((int)Lang::De, (int)r.lang);  // German by default
  TEST_ASSERT_EQUAL(0, (int)r.stops.size());
  TEST_ASSERT_FALSE(r.touchCalValid);
  TEST_ASSERT_FALSE(settingsFromJson("{not json", r));
  TEST_ASSERT_FALSE(settingsFromJson(nullptr, r));
}

void test_clamping_and_limits() {
  const char* j =
      "{\"stops\":["
      "{\"id\":\"1\",\"walk\":200,\"types\":0},{\"id\":\"2\"},{\"id\":\"\"},{\"id\":\"3\"},"
      "{\"id\":\"4\"},{\"id\":\"5\"}],"
      "\"display\":{\"brightness\":0},\"night\":{\"start\":5000,\"mode\":\"bogus\"},"
      "\"weather\":{\"override\":true,\"lat\":0,\"lon\":0}}";
  Settings r;
  TEST_ASSERT_TRUE(settingsFromJson(j, r));
  TEST_ASSERT_EQUAL(kMaxStops, (int)r.stops.size());
  TEST_ASSERT_EQUAL(60, r.stops[0].walkMin);
  TEST_ASSERT_EQUAL(kAllTypes, r.stops[0].typeMask);
  TEST_ASSERT_EQUAL_STRING("4", r.stops[3].id.c_str());  // empty id skipped
  TEST_ASSERT_EQUAL(5, r.brightness);
  TEST_ASSERT_EQUAL(1439, r.nightStart);
  TEST_ASSERT_EQUAL((int)NightMode::Dim, (int)r.nightMode);
  TEST_ASSERT_FALSE(r.weatherOverride);  // 0/0 rejected
}

void test_partial_update_keeps_calibration() {
  Settings s;
  s.touchCalValid = true;
  s.touchCal[0] = 1234;
  s.brightness = 70;
  TEST_ASSERT_TRUE(applySettingsUpdate("{\"lang\":\"en\",\"touchCal\":[1,2,3,4,5,6,7,8]}", s));
  TEST_ASSERT_EQUAL((int)Lang::En, (int)s.lang);
  TEST_ASSERT_EQUAL(1234, s.touchCal[0]);
  TEST_ASSERT_EQUAL(70, s.brightness);
  TEST_ASSERT_FALSE(applySettingsUpdate("[1,2]", s));
}

void test_weather_location_fallback() {
  Settings s;
  float lat, lon;
  TEST_ASSERT_FALSE(s.weatherLocation(lat, lon));
  StopConfig st;
  st.id = "x";
  st.lat = 48.06f;
  st.lon = 11.37f;
  s.stops.push_back(st);
  TEST_ASSERT_TRUE(s.weatherLocation(lat, lon));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 48.06, lat);
  s.weatherOverride = true;
  s.weatherLat = 47.0f;
  TEST_ASSERT_TRUE(s.weatherLocation(lat, lon));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 47.0, lat);
}

void test_night_window() {
  Settings s;
  s.nightStart = 22 * 60;
  s.nightEnd = 6 * 60 + 30;
  TEST_ASSERT_TRUE(s.isNight(23 * 60));
  TEST_ASSERT_TRUE(s.isNight(2 * 60));
  TEST_ASSERT_FALSE(s.isNight(6 * 60 + 30));
  TEST_ASSERT_FALSE(s.isNight(12 * 60));
  s.nightStart = 1 * 60;
  s.nightEnd = 5 * 60;
  TEST_ASSERT_TRUE(s.isNight(3 * 60));
  TEST_ASSERT_FALSE(s.isNight(23 * 60));
  s.nightMode = NightMode::Off;
  TEST_ASSERT_FALSE(s.isNight(3 * 60));
}

void test_default_label() {
  TEST_ASSERT_EQUAL_STRING("Marienplatz", defaultStopLabel("München, Marienplatz").c_str());
  std::string l = defaultStopLabel("Garching, Forschungszentrum");
  TEST_ASSERT_TRUE(l.size() <= 14);
  TEST_ASSERT_EQUAL_STRING("Garching", l.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_defaults_and_garbage);
  RUN_TEST(test_clamping_and_limits);
  RUN_TEST(test_partial_update_keeps_calibration);
  RUN_TEST(test_weather_location_fallback);
  RUN_TEST(test_night_window);
  RUN_TEST(test_default_label);
  return UNITY_END();
}
