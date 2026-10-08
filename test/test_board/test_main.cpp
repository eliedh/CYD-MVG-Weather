#include <string.h>
#include <unity.h>

#include "../fixture_util.h"
#include "board.h"
#include "line_style.h"
#include "mvg_parser.h"

using namespace core;

static StopDepartures g_stops[kMaxStops];
static Settings g_settings;

static void load(int idx, const char* fixture) {
  std::string j = readFixture(fixture);
  MemorySource src(j);
  TEST_ASSERT_TRUE(parseDepartures(src, (uint8_t)idx, g_stops[idx]));
  g_stops[idx].valid = true;
}

void setUp() {
  for (auto& s : g_stops) s = StopDepartures();
  g_settings = Settings();
}
void tearDown() {}

static StopConfig stop(const char* id, uint8_t walk) {
  StopConfig s;
  s.id = id;
  s.name = id;
  s.label = id;
  s.walkMin = walk;
  return s;
}

void test_single_stop_no_walk() {
  g_settings.stops.push_back(stop("freiheit", 0));
  load(0, "mvg_departures_freiheit.json");
  BoardRow rows[32];
  int n = buildBoard(g_stops, g_settings, FIXTURE_NOW, rows, 32);
  // 16 departures, the one at -1 min has left.
  TEST_ASSERT_EQUAL(15, n);
  TEST_ASSERT_EQUAL_STRING("U3", rows[0].dep.label);
  TEST_ASSERT_EQUAL(0, rows[0].minutes);
  TEST_ASSERT_TRUE(rows[0].highlight);
  TEST_ASSERT_EQUAL(-1, rows[0].leaveInMin);  // no walk time -> no "leave in"
  for (int i = 1; i < n; i++) {
    TEST_ASSERT_FALSE(rows[i].highlight);
    TEST_ASSERT_TRUE(rows[i - 1].dep.effectiveTime() <= rows[i].dep.effectiveTime());
  }
}

void test_walk_time_hides_unreachable_and_sets_leave_in() {
  g_settings.stops.push_back(stop("freiheit", 5));
  load(0, "mvg_departures_freiheit.json");
  BoardRow rows[32];
  int n = buildBoard(g_stops, g_settings, FIXTURE_NOW, rows, 32);
  for (int i = 0; i < n; i++) TEST_ASSERT_TRUE(rows[i].secondsUntil >= 5 * 60);
  // first reachable: cancelled U6 at +6 is never the highlight; U3 Moosach +5 delayed 2 => +7,
  // so order: U6(+6, cancelled) U3(+7) ...
  TEST_ASSERT_EQUAL_STRING("U6", rows[0].dep.label);
  TEST_ASSERT_TRUE(rows[0].dep.cancelled);
  TEST_ASSERT_FALSE(rows[0].highlight);
  TEST_ASSERT_EQUAL_STRING("U3", rows[1].dep.label);
  TEST_ASSERT_TRUE(rows[1].highlight);
  TEST_ASSERT_EQUAL(7, rows[1].minutes);
  TEST_ASSERT_EQUAL(2, rows[1].leaveInMin);
}

void test_merge_two_stops_sorted_with_stop_index() {
  g_settings.stops.push_back(stop("freiheit", 0));
  g_settings.stops.push_back(stop("gauting", 0));
  load(0, "mvg_departures_freiheit.json");
  load(1, "mvg_departures_gauting.json");
  BoardRow rows[64];
  int n = buildBoard(g_stops, g_settings, FIXTURE_NOW, rows, 64);
  TEST_ASSERT_EQUAL(15 + 7, n);
  bool sawGauting = false;
  for (int i = 0; i < n; i++) {
    if (i) TEST_ASSERT_TRUE(rows[i - 1].dep.effectiveTime() <= rows[i].dep.effectiveTime());
    if (rows[i].dep.stopIndex == 1) sawGauting = true;
  }
  TEST_ASSERT_TRUE(sawGauting);
  TEST_ASSERT_EQUAL_STRING("S6", rows[1].dep.label);  // +1 min
  TEST_ASSERT_EQUAL(1, rows[1].dep.stopIndex);
}

void test_max_rows() {
  g_settings.stops.push_back(stop("freiheit", 0));
  load(0, "mvg_departures_freiheit.json");
  BoardRow rows[4];
  TEST_ASSERT_EQUAL(4, buildBoard(g_stops, g_settings, FIXTURE_NOW, rows, 4));
}

void test_filters_line_direction_and_type() {
  StopConfig s = stop("freiheit", 0);
  s.excludes.push_back("U6");                   // whole line
  s.excludes.push_back("U3>Fürstenried West");  // one direction
  s.typeMask = (uint8_t)(kAllTypes & ~typeBit(TransportType::Bus));
  g_settings.stops.push_back(s);
  load(0, "mvg_departures_freiheit.json");
  BoardRow rows[32];
  int n = buildBoard(g_stops, g_settings, FIXTURE_NOW, rows, 32);
  for (int i = 0; i < n; i++) {
    TEST_ASSERT_TRUE(strcmp(rows[i].dep.label, "U6") != 0);
    TEST_ASSERT_FALSE(rows[i].dep.type == TransportType::Bus);
    if (strcmp(rows[i].dep.label, "U3") == 0)
      TEST_ASSERT_EQUAL_STRING("Moosach", rows[i].dep.destination);
  }
  TEST_ASSERT_EQUAL(4, n);  // tram 23 x2, U3 Moosach x2
}

void test_invalid_stop_data_ignored_and_extra_stops() {
  g_settings.stops.push_back(stop("a", 0));
  g_settings.stops.push_back(stop("b", 0));
  load(0, "mvg_departures_gauting.json");
  g_stops[1].count = 3;  // data but never valid
  BoardRow rows[32];
  TEST_ASSERT_EQUAL(7, buildBoard(g_stops, g_settings, FIXTURE_NOW, rows, 32));
}

void test_countdown_advances() {
  g_settings.stops.push_back(stop("gauting", 0));
  load(0, "mvg_departures_gauting.json");
  BoardRow rows[8];
  buildBoard(g_stops, g_settings, FIXTURE_NOW + 59, rows, 8);
  TEST_ASSERT_EQUAL(0, rows[0].minutes);  // 1 s left
  buildBoard(g_stops, g_settings, FIXTURE_NOW + 61, rows, 8);
  TEST_ASSERT_EQUAL_STRING("965", rows[0].dep.label);  // S6 has left
  TEST_ASSERT_EQUAL(2, rows[0].minutes);
}

void test_board_lines_and_directions() {
  g_settings.stops.push_back(stop("freiheit", 0));
  g_settings.stops.back().excludes.push_back("N40");
  load(0, "mvg_departures_freiheit.json");
  auto lines = boardLines(g_stops, g_settings);
  TEST_ASSERT_EQUAL(5, (int)lines.size());  // U6 U3 23 59 142 (N40 excluded)
  auto dirs = distinctLineDirections(g_stops[0]);
  // U3: Fürstenried West, Moosach; U6: Großhadern, Garching; 23 x2; 59; 142; N40
  TEST_ASSERT_EQUAL(9, (int)dirs.size());
  TEST_ASSERT_EQUAL_STRING("U3", dirs[0].label.c_str());
}

void test_line_styles() {
  LineStyle u3 = lineStyle(TransportType::UBahn, "U3");
  TEST_ASSERT_EQUAL_HEX32(0xF36E31, u3.bg);
  TEST_ASSERT_EQUAL((int)BadgeShape::Rect, (int)u3.shape);
  LineStyle u7 = lineStyle(TransportType::UBahn, "U7");
  TEST_ASSERT_NOT_EQUAL(u7.bg, u7.bg2);
  LineStyle s8 = lineStyle(TransportType::SBahn, "S8");
  TEST_ASSERT_EQUAL_HEX32(0xFFCB06, s8.fg);
  TEST_ASSERT_EQUAL((int)BadgeShape::Pill, (int)s8.shape);
  LineStyle n = lineStyle(TransportType::Bus, "N40");
  LineStyle b = lineStyle(TransportType::Bus, "59");
  TEST_ASSERT_NOT_EQUAL(n.bg, b.bg);
  lineStyle(TransportType::Other, nullptr);  // must not crash
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_single_stop_no_walk);
  RUN_TEST(test_walk_time_hides_unreachable_and_sets_leave_in);
  RUN_TEST(test_merge_two_stops_sorted_with_stop_index);
  RUN_TEST(test_max_rows);
  RUN_TEST(test_filters_line_direction_and_type);
  RUN_TEST(test_invalid_stop_data_ignored_and_extra_stops);
  RUN_TEST(test_countdown_advances);
  RUN_TEST(test_board_lines_and_directions);
  RUN_TEST(test_line_styles);
  return UNITY_END();
}
