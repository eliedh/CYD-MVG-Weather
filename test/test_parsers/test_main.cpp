#include <string.h>
#include <unity.h>

#include <algorithm>

#include "../fixture_util.h"
#include "mvg_parser.h"
#include "providers.h"
#include "text_util.h"
#include "weather_parser.h"

using namespace core;

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- locations

void test_locations_parses_stations_and_skips_pois() {
  std::string j = readFixture("mvg_locations_freiheit.json");
  TEST_ASSERT_FALSE(j.empty());
  MemorySource src(j);
  std::vector<StopInfo> out;
  TEST_ASSERT_TRUE(parseLocations(src, out));
  TEST_ASSERT_EQUAL(3, (int)out.size());
  TEST_ASSERT_EQUAL_STRING("Münchner Freiheit", out[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("de:09162:500", out[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("München", out[0].place.c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 48.16153, out[0].lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 11.58630, out[0].lon);
  TEST_ASSERT_TRUE(out[0].typeMask & typeBit(TransportType::UBahn));
  TEST_ASSERT_TRUE(out[0].typeMask & typeBit(TransportType::Tram));
  TEST_ASSERT_FALSE(out[0].typeMask & typeBit(TransportType::SBahn));
  TEST_ASSERT_EQUAL_STRING("Gauting, Bahnhof", out[2].name.c_str());
  TEST_ASSERT_TRUE(out[2].typeMask & typeBit(TransportType::RegionalBus));
}

void test_locations_max_results() {
  std::string j = readFixture("mvg_locations_freiheit.json");
  MemorySource src(j);
  std::vector<StopInfo> out;
  TEST_ASSERT_TRUE(parseLocations(src, out, 1));
  TEST_ASSERT_EQUAL(1, (int)out.size());
}

void test_locations_empty_and_garbage() {
  std::vector<StopInfo> out;
  MemorySource empty("[]", 2);
  TEST_ASSERT_TRUE(parseLocations(empty, out));
  TEST_ASSERT_EQUAL(0, (int)out.size());
  MemorySource ws("  \n [ ] ", 8);
  TEST_ASSERT_TRUE(parseLocations(ws, out));
  MemorySource html("<html>502 Bad Gateway</html>", 28);
  TEST_ASSERT_FALSE(parseLocations(html, out));
  MemorySource trunc("[{\"type\":\"STATION\",\"name\":\"X\"", 30);
  TEST_ASSERT_FALSE(parseLocations(trunc, out));
}

// ---------------------------------------------------------------- departures

void test_departures_fields() {
  std::string j = readFixture("mvg_departures_freiheit.json");
  ChunkedSource src(j);
  StopDepartures sd;
  TEST_ASSERT_TRUE(parseDepartures(src, 1, sd));
  TEST_ASSERT_EQUAL(16, sd.count);
  const Departure& d0 = sd.items[0];
  TEST_ASSERT_EQUAL_STRING("U6", d0.label);
  TEST_ASSERT_EQUAL_STRING("Klinikum Großhadern", d0.destination);
  TEST_ASSERT_EQUAL((int)TransportType::UBahn, (int)d0.type);
  TEST_ASSERT_EQUAL(FIXTURE_NOW - 60, d0.plannedTime);
  TEST_ASSERT_TRUE(d0.realtime);
  TEST_ASSERT_EQUAL_STRING("2", d0.platform);
  TEST_ASSERT_EQUAL(1, d0.stopIndex);

  const Departure& tram = sd.items[2];
  TEST_ASSERT_EQUAL((int)TransportType::Tram, (int)tram.type);
  TEST_ASSERT_EQUAL(1, tram.delayMin);
  TEST_ASSERT_EQUAL(FIXTURE_NOW + 3 * 60, tram.effectiveTime());

  const Departure& bus = sd.items[4];
  TEST_ASSERT_FALSE(bus.realtime);
  TEST_ASSERT_EQUAL(FIXTURE_NOW + 4 * 60, bus.effectiveTime());

  TEST_ASSERT_TRUE(sd.items[6].cancelled);
  TEST_ASSERT_EQUAL_STRING("N40", sd.items[14].label);
}

void test_departures_regional() {
  std::string j = readFixture("mvg_departures_gauting.json");
  MemorySource src(j);
  StopDepartures sd;
  TEST_ASSERT_TRUE(parseDepartures(src, 0, sd));
  TEST_ASSERT_EQUAL(7, sd.count);
  TEST_ASSERT_EQUAL((int)TransportType::SBahn, (int)sd.items[0].type);
  TEST_ASSERT_EQUAL((int)TransportType::RegionalBus, (int)sd.items[1].type);
  TEST_ASSERT_EQUAL(3, sd.items[2].delayMin);
}

void test_departures_alternative_shape() {
  std::string j = readFixture("mvg_departures_alt_shape.json");
  MemorySource src(j);
  StopDepartures sd;
  TEST_ASSERT_TRUE(parseDepartures(src, 2, sd));
  TEST_ASSERT_EQUAL(2, sd.count);  // third entry has no time -> dropped
  TEST_ASSERT_EQUAL_STRING("S8", sd.items[0].label);
  TEST_ASSERT_EQUAL_STRING("Flughafen München", sd.items[0].destination);
  TEST_ASSERT_EQUAL(FIXTURE_NOW + 5 * 60, sd.items[0].plannedTime);
  TEST_ASSERT_EQUAL(FIXTURE_NOW + 7 * 60, sd.items[0].effectiveTime());
  TEST_ASSERT_EQUAL(2, sd.items[0].delayMin);
  TEST_ASSERT_EQUAL_STRING("3", sd.items[0].platform);
  TEST_ASSERT_EQUAL((int)TransportType::Tram, (int)sd.items[1].type);
  TEST_ASSERT_EQUAL_STRING("17", sd.items[1].label);
}

void test_departures_limit_and_long_names() {
  std::string j = "[";
  for (int i = 0; i < 30; i++) {
    if (i) j += ",";
    j += "{\"plannedDepartureTime\":" + std::to_string((FIXTURE_NOW + i * 60) * 1000LL) +
         ",\"label\":\"X300000000\",\"transportType\":\"BUS\",\"destination\":"
         "\"Ein wirklich sehr sehr langer Zielname der nicht passt äöü\"}";
  }
  j += "]";
  MemorySource src(j);
  StopDepartures sd;
  TEST_ASSERT_TRUE(parseDepartures(src, 0, sd));
  TEST_ASSERT_EQUAL(kMaxDeparturesPerStop, sd.count);
  TEST_ASSERT_TRUE(strlen(sd.items[0].label) < sizeof(sd.items[0].label));
  TEST_ASSERT_TRUE(strlen(sd.items[0].destination) < sizeof(sd.items[0].destination));
}

// ---------------------------------------------------------------- messages

void test_messages_filter_by_line_and_validity() {
  std::string j = readFixture("mvg_messages.json");
  MemorySource src(j);
  std::vector<LineKey> lines(3);
  strcpy(lines[0].label, "U3");
  lines[0].type = TransportType::UBahn;
  strcpy(lines[1].label, "23");
  lines[1].type = TransportType::Tram;
  strcpy(lines[2].label, "S6");
  lines[2].type = TransportType::SBahn;
  ServiceMessage msgs[kMaxMessages];
  int n = 0;
  TEST_ASSERT_TRUE(parseMessages(src, lines, FIXTURE_NOW, msgs, kMaxMessages, n));
  // U3/U6 (valid), S6 (no validTo). Tram 23 expired; bus 23 type mismatch;
  // bus 100 not displayed; duplicate title dropped.
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL_STRING("U3/U6: Eingeschränkter Betrieb am Abend", msgs[0].title);
  TEST_ASSERT_EQUAL_STRING("U3", msgs[0].lines);
  TEST_ASSERT_NOT_NULL(strstr(msgs[0].text, "„Implerstraße“"));
  TEST_ASSERT_NOT_NULL(strstr(msgs[0].text, "26. Oktober"));
  TEST_ASSERT_NOT_NULL(strstr(msgs[0].text, "Lindwurmstraße"));
  TEST_ASSERT_NULL(strstr(msgs[0].text, "<"));
  TEST_ASSERT_EQUAL_STRING("S6", msgs[1].lines);
}

void test_messages_max_out() {
  std::string j = readFixture("mvg_messages.json");
  MemorySource src(j);
  std::vector<LineKey> lines(1);
  strcpy(lines[0].label, "S6");
  lines[0].type = TransportType::Other;  // type unknown -> match by label only
  ServiceMessage msgs[1];
  int n = 0;
  TEST_ASSERT_TRUE(parseMessages(src, lines, FIXTURE_NOW, msgs, 1, n));
  TEST_ASSERT_EQUAL(1, n);
}

// ---------------------------------------------------------------- weather

void test_openmeteo() {
  std::string j = readFixture("openmeteo_munich.json");
  ChunkedSource src(j, 13);
  WeatherData w;
  TEST_ASSERT_TRUE(parseOpenMeteo(src, FIXTURE_NOW, w));
  TEST_ASSERT_TRUE(w.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 12.6, w.temp);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 10.9, w.feelsLike);
  TEST_ASSERT_EQUAL(3, w.code);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 15.2, w.tMax);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 7.9, w.tMin);
  TEST_ASSERT_EQUAL(60, w.precipMaxToday);
  TEST_ASSERT_EQUAL(kMaxHours, w.nHours);
  // Hourly starts at the hour containing NOW (fixture starts 2h earlier).
  TEST_ASSERT_EQUAL(FIXTURE_NOW, w.hours[0].time);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 12.4, w.hours[0].temp);
  TEST_ASSERT_EQUAL(20, w.hours[0].precipProb);
  // Max over next 6 hours: 20,35,55,60,40,20 -> 60
  TEST_ASSERT_EQUAL(60, w.rainNextHours);
  TEST_ASSERT_EQUAL(61, w.hours[1].code);
  TEST_ASSERT_FALSE(w.hours[0].isDay);
}

void test_openmeteo_missing_blocks() {
  const char* j = "{\"current\":{\"temperature_2m\":-3.5,\"weather_code\":71}}";
  MemorySource src(j, strlen(j));
  WeatherData w;
  TEST_ASSERT_TRUE(parseOpenMeteo(src, FIXTURE_NOW, w));
  TEST_ASSERT_FLOAT_WITHIN(0.01, -3.5, w.temp);
  TEST_ASSERT_FLOAT_WITHIN(0.01, -3.5, w.tMax);
  TEST_ASSERT_EQUAL(0, w.nHours);
  TEST_ASSERT_EQUAL((int)WeatherIcon::Snow, (int)weatherIconForCode(w.code));

  const char* bad = "{\"error\":true,\"reason\":\"Latitude must be in range\"}";
  MemorySource src2(bad, strlen(bad));
  TEST_ASSERT_FALSE(parseOpenMeteo(src2, FIXTURE_NOW, w));
}

// ---------------------------------------------------------------- utils/urls

void test_html_to_text() {
  char out[128];
  htmlToText("<p>A&amp;B&nbsp; &auml;&#246;&#xFC;</p><p>Zeile&shy;2</p>", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("A&B äöü\nZeile2", out);
  htmlToText("  viel    Leerraum \n\n hier ", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("viel Leerraum hier", out);
  // truncation never splits UTF-8
  htmlToText("ääääää", out, 6);
  TEST_ASSERT_EQUAL_STRING("ää", out);
}

void test_copy_utf8_truncation() {
  char buf[6];
  copyUtf8(buf, sizeof(buf), "Größe");
  TEST_ASSERT_EQUAL_STRING("Grö", buf);  // "ß" would not fit completely
}

void test_urls() {
  std::string u = MvgProvider::searchUrl("Münchner Freiheit");
  TEST_ASSERT_EQUAL_STRING(
      "https://www.mvg.de/api/bgw-pt/v3/locations?query=M%C3%BCnchner%20Freiheit&locationTypes=STATION",
      u.c_str());
  StopConfig st;
  st.id = "de:09162:500";
  u = MvgProvider::departuresUrl(st, 20);
  TEST_ASSERT_EQUAL_STRING(
      "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=de%3A09162%3A500&limit=20", u.c_str());
  st.typeMask = typeBit(TransportType::UBahn) | typeBit(TransportType::Bus);
  u = MvgProvider::departuresUrl(st, 10);
  TEST_ASSERT_NOT_NULL(strstr(u.c_str(), "&transportTypes=UBAHN,BUS"));
  std::string w = openMeteoUrl(48.1615f, 11.5863f);
  TEST_ASSERT_NOT_NULL(strstr(w.c_str(), "latitude=48.1615&longitude=11.5863"));
  TEST_ASSERT_NOT_NULL(strstr(w.c_str(), "timezone=Europe%2FBerlin"));
}

// Provider end-to-end through a fake fetcher.
class FakeHttp : public HttpFetcher {
 public:
  std::string lastUrl;
  std::string body;
  int status = 200;

 protected:
  bool doGet(const char* url, Consumer& consume) override {
    lastUrl = url;
    lastStatus_ = status;
    if (status != 200) return false;
    MemorySource s(body);
    return consume(s);
  }
};

void test_provider_with_fake_http() {
  FakeHttp http;
  MvgProvider p;
  http.body = readFixture("mvg_departures_gauting.json");
  StopConfig st;
  st.id = "de:09188:7520";
  StopDepartures sd;
  TEST_ASSERT_TRUE(p.fetchDepartures(http, st, 3, 20, sd));
  TEST_ASSERT_EQUAL(7, sd.count);
  TEST_ASSERT_EQUAL(3, sd.items[0].stopIndex);
  http.status = 503;
  TEST_ASSERT_FALSE(p.fetchDepartures(http, st, 3, 20, sd));
  TEST_ASSERT_EQUAL(503, http.lastStatus());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_locations_parses_stations_and_skips_pois);
  RUN_TEST(test_locations_max_results);
  RUN_TEST(test_locations_empty_and_garbage);
  RUN_TEST(test_departures_fields);
  RUN_TEST(test_departures_regional);
  RUN_TEST(test_departures_alternative_shape);
  RUN_TEST(test_departures_limit_and_long_names);
  RUN_TEST(test_messages_filter_by_line_and_validity);
  RUN_TEST(test_messages_max_out);
  RUN_TEST(test_openmeteo);
  RUN_TEST(test_openmeteo_missing_blocks);
  RUN_TEST(test_html_to_text);
  RUN_TEST(test_copy_utf8_truncation);
  RUN_TEST(test_urls);
  RUN_TEST(test_provider_with_fake_http);
  return UNITY_END();
}
