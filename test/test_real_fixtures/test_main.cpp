// Checks the parsers against REAL captures in test/fixtures/real/ (created by
// tools/fetch_fixtures.sh). Each test is ignored when its capture is missing.
#include <unity.h>

#include "../fixture_util.h"
#include "mvg_parser.h"
#include "weather_parser.h"

using namespace core;

void setUp() {}
void tearDown() {}

void test_real_locations() {
  std::string j = readFixture("real/mvg_locations.json");
  if (j.empty()) TEST_IGNORE_MESSAGE("no real capture; run tools/fetch_fixtures.sh");
  MemorySource src(j);
  std::vector<StopInfo> out;
  TEST_ASSERT_TRUE(parseLocations(src, out));
  TEST_ASSERT_TRUE(out.size() > 0);
  TEST_ASSERT_TRUE(out[0].id.size() > 3);
  TEST_ASSERT_TRUE(out[0].lat > 47 && out[0].lat < 49);
}

void test_real_departures() {
  std::string j = readFixture("real/mvg_departures.json");
  if (j.empty()) TEST_IGNORE_MESSAGE("no real capture; run tools/fetch_fixtures.sh");
  MemorySource src(j);
  StopDepartures sd;
  TEST_ASSERT_TRUE(parseDepartures(src, 0, sd));
  TEST_ASSERT_TRUE(sd.count > 0);
  for (int i = 0; i < sd.count; i++) {
    TEST_ASSERT_TRUE(sd.items[i].plannedTime > 1600000000);
    TEST_ASSERT_TRUE(sd.items[i].label[0] != 0);
    TEST_ASSERT_TRUE(sd.items[i].type != TransportType::Other);
  }
}

void test_real_messages() {
  std::string j = readFixture("real/mvg_messages.json");
  if (j.empty()) TEST_IGNORE_MESSAGE("no real capture; run tools/fetch_fixtures.sh");
  // Match every U-Bahn/S-Bahn line so that something is found.
  std::vector<LineKey> lines;
  for (const char* l : {"U1", "U2", "U3", "U4", "U5", "U6", "U7", "U8", "S1", "S2", "S3", "S4",
                        "S6", "S7", "S8", "S20"}) {
    LineKey k;
    strcpy(k.label, l);
    lines.push_back(k);
  }
  ServiceMessage m[kMaxMessages];
  int n = 0;
  MemorySource src(j);
  TEST_ASSERT_TRUE(parseMessages(src, lines, 0, m, kMaxMessages, n));
  TEST_MESSAGE(n ? "messages matched" : "no message matched (can be legit)");
}

void test_real_weather() {
  std::string j = readFixture("real/openmeteo.json");
  if (j.empty()) TEST_IGNORE_MESSAGE("no real capture; run tools/fetch_fixtures.sh");
  MemorySource src(j);
  WeatherData w;
  TEST_ASSERT_TRUE(parseOpenMeteo(src, 0, w));
  TEST_ASSERT_TRUE(w.valid);
  TEST_ASSERT_TRUE(w.nHours > 0);
  TEST_ASSERT_TRUE(w.temp > -40 && w.temp < 50);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_real_locations);
  RUN_TEST(test_real_departures);
  RUN_TEST(test_real_messages);
  RUN_TEST(test_real_weather);
  return UNITY_END();
}
