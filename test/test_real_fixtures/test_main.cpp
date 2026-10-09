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

// Real capture from 2026-10-09 (306 messages, 380 kB), fed in 3-byte chunks
// like a slow TLS stream.
static const int64_t kCaptureNow = 1791565200;

void test_real_messages_u3_incident() {
  std::string j = readFixture("real/mvg_messages.json");
  if (j.empty()) TEST_IGNORE_MESSAGE("no real capture");
  std::vector<LineKey> lines(1);
  strcpy(lines[0].label, "U3");
  lines[0].type = TransportType::UBahn;
  ServiceMessage m[kMaxMessages];
  int n = 0;
  ChunkedSource src(j, 3);
  TEST_ASSERT_TRUE(parseMessages(src, lines, kCaptureNow, m, kMaxMessages, n));
  TEST_ASSERT_TRUE(n >= 1);
  TEST_ASSERT_TRUE(m[0].incident);
  TEST_ASSERT_NOT_NULL(strstr(m[0].title, "Sendlinger Tor"));
  TEST_ASSERT_TRUE(strlen(m[0].text) > 50);
  TEST_ASSERT_NULL(strstr(m[0].text, "<"));
}

void test_real_messages_cap_keeps_incident() {
  std::string j = readFixture("real/mvg_messages.json");
  if (j.empty()) TEST_IGNORE_MESSAGE("no real capture");
  // N41 has ~10 schedule changes; bus 53 has an incident later in the feed.
  std::vector<LineKey> lines(2);
  strcpy(lines[0].label, "N41");
  lines[0].type = TransportType::Bus;
  strcpy(lines[1].label, "53");
  lines[1].type = TransportType::Bus;
  ServiceMessage m[kMaxMessages];
  int n = 0;
  ChunkedSource src(j, 3);
  TEST_ASSERT_TRUE(parseMessages(src, lines, kCaptureNow, m, kMaxMessages, n));
  TEST_ASSERT_EQUAL(kMaxMessages, n);
  TEST_ASSERT_TRUE(m[0].incident);
  TEST_ASSERT_NOT_NULL(strstr(m[0].lines, "53"));
  for (int i = 1; i < n; i++) TEST_ASSERT_FALSE(m[i].incident);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_real_locations);
  RUN_TEST(test_real_departures);
  RUN_TEST(test_real_messages);
  RUN_TEST(test_real_messages_u3_incident);
  RUN_TEST(test_real_messages_cap_keeps_incident);
  RUN_TEST(test_real_weather);
  return UNITY_END();
}
