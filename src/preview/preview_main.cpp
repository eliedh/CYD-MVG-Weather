// Host-side design preview: renders every screen from the test fixtures
// through the same band renderer the device uses, and writes PNGs to
// docs/screenshots/ (1x and 2x).
//
//   pio run -e preview && .pio/build/preview/program [output_dir]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <fstream>
#include <sstream>
#include <string>

#include <LovyanGFX.hpp>

#include "band_renderer.h"
#include "fonts.h"
#include "i18n.h"
#include "icons.h"
#include "mvg_parser.h"
#include "screens.h"
#include "theme.h"
#include "vm_builder.h"
#include "weather_parser.h"

using namespace core;

static const int64_t kNow = 1791475200 + 25;  // 2026-10-08 18:00:25 CEST
static std::string g_out = "docs/screenshots";

static std::string readFile(const std::string& name) {
  for (const char* d : {"test/fixtures/", "../test/fixtures/"}) {
    std::ifstream f(std::string(d) + name, std::ios::binary);
    if (f) {
      std::stringstream ss;
      ss << f.rdbuf();
      return ss.str();
    }
  }
  fprintf(stderr, "fixture %s not found (run from the project root)\n", name.c_str());
  exit(1);
}

static void loadDepartures(DataSnapshot& d, int idx, const char* file) {
  std::string j = readFile(file);
  MemorySource src(j);
  parseDepartures(src, (uint8_t)idx, d.stops[idx]);
  d.stops[idx].valid = true;
  d.stops[idx].fetchedAt = kNow - 20;
}

static void loadWeather(DataSnapshot& d) {
  std::string j = readFile("openmeteo_munich.json");
  MemorySource src(j);
  parseOpenMeteo(src, kNow, d.weather);
}

static void loadMessages(DataSnapshot& d, const Settings& s) {
  std::string j = readFile("mvg_messages.json");
  MemorySource src(j);
  auto lines = boardLines(d.stops, s);
  parseMessages(src, lines, kNow, d.messages, kMaxMessages, d.messageCount);
}

static StopConfig stop(const char* id, const char* name, uint8_t walk) {
  StopConfig s;
  s.id = id;
  s.name = name;
  s.label = defaultStopLabel(name);
  s.walkMin = walk;
  return s;
}

static bool writeFile(const std::string& path, const void* data, size_t len) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  fwrite(data, 1, len, f);
  fclose(f);
  return true;
}

static void savePng(LGFX_Sprite& full, const std::string& name) {
  size_t len = 0;
  void* png = full.createPng(&len, 0, 0, full.width(), full.height());
  if (!png || !writeFile(g_out + "/" + name + ".png", png, len)) {
    fprintf(stderr, "failed to write %s\n", name.c_str());
    exit(1);
  }
  free(png);
  // 2x nearest-neighbour copy for comfortable review on high-DPI screens
  LGFX_Sprite big;
  big.setColorDepth(24);
  big.createSprite(full.width() * 2, full.height() * 2);
  for (int y = 0; y < full.height(); y++)
    for (int x = 0; x < full.width(); x++) {
      auto px = full.readPixelRGB(x, y);
      big.fillRect(x * 2, y * 2, 2, 2, (uint32_t)lgfx::color888(px.R8(), px.G8(), px.B8()));
    }
  png = big.createPng(&len, 0, 0, big.width(), big.height());
  writeFile(g_out + "/" + name + "@2x.png", png, len);
  free(png);
  printf("  %s.png\n", name.c_str());
}

static void render(ui::BandRenderer& r, LGFX_Sprite& full, const ui::ViewModel& vm,
                   const std::string& name) {
  r.invalidate();
  int pushed = r.render([&](ui::Painter& p) { ui::drawScreen(p, vm); });
  (void)pushed;
  savePng(full, name);
}

static void baseVm(ui::ViewModel& vm) {
  strcpy(vm.apSsid, "Abfahrt-Setup-7F3A");
  strcpy(vm.homeSsid, "FRITZ!Box 7590 KL");
  strcpy(vm.ip, "192.168.178.47");
  strcpy(vm.host, "abfahrt.local");
}

int main(int argc, char** argv) {
  if (argc > 1) g_out = argv[1];
  // mkdir -p
  for (size_t i = 1; i <= g_out.size(); i++)
    if (i == g_out.size() || g_out[i] == '/') mkdir(g_out.substr(0, i).c_str(), 0755);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  LGFX_Sprite full;
  full.setColorDepth(16);
  full.createSprite(ui::theme::kW, ui::theme::kH);
  ui::BandRenderer renderer;
  renderer.begin(&full);

  // ---- data
  Settings one;
  one.stops.push_back(stop("de:09162:500", "Münchner Freiheit", 0));
  DataSnapshot d1;
  loadDepartures(d1, 0, "mvg_departures_freiheit.json");
  loadWeather(d1);
  loadMessages(d1, one);
  d1.departuresFetchedAt = kNow - 20;

  Settings two;
  two.stops.push_back(stop("de:09162:500", "Münchner Freiheit", 3));
  two.stops.push_back(stop("de:09188:7520", "Gauting, Bahnhof", 6));
  DataSnapshot d2;
  loadDepartures(d2, 0, "mvg_departures_freiheit.json");
  loadDepartures(d2, 1, "mvg_departures_gauting.json");
  loadWeather(d2);
  d2.departuresFetchedAt = kNow - 20;

  printf("Rendering to %s/\n", g_out.c_str());
  ui::ViewModel vm;
  baseVm(vm);

  // main screens
  vm.screen = ui::Screen::Main;
  ui::fillFromSnapshot(vm, one, d1, kNow, true);
  render(renderer, full, vm, "01_main_one_stop_notice");

  ui::fillFromSnapshot(vm, two, d2, kNow, true);
  render(renderer, full, vm, "02_main_two_stops_walk");

  two.lang = Lang::En;
  ui::fillFromSnapshot(vm, two, d2, kNow, true);
  render(renderer, full, vm, "03_main_two_stops_en");
  two.lang = Lang::De;

  {
    DataSnapshot stale = d2;
    stale.departuresError = true;
    stale.departuresFetchedAt = kNow - 7 * 60;
    ui::fillFromSnapshot(vm, two, stale, kNow, true);
    render(renderer, full, vm, "04_main_stale");
    vm.wifiDown = true;
    render(renderer, full, vm, "05_main_wifi_lost");
    vm.wifiDown = false;
  }
  {
    DataSnapshot empty;
    loadWeather(empty);
    ui::fillFromSnapshot(vm, one, empty, kNow, true);
    render(renderer, full, vm, "06_main_loading");
    empty.departuresError = true;
    ui::fillFromSnapshot(vm, one, empty, kNow, true);
    render(renderer, full, vm, "07_main_api_error");
    DataSnapshot noWx = d2;
    noWx.weather = WeatherData();
    ui::fillFromSnapshot(vm, two, noWx, kNow + 3600 * 5, true);
    render(renderer, full, vm, "08_main_late_no_weather");
  }

  // details
  ui::fillFromSnapshot(vm, one, d1, kNow, true);
  vm.screen = ui::Screen::WeatherDetail;
  render(renderer, full, vm, "10_weather_detail");
  one.lang = Lang::En;
  ui::fillFromSnapshot(vm, one, d1, kNow, true);
  render(renderer, full, vm, "11_weather_detail_en");
  one.lang = Lang::De;
  ui::fillFromSnapshot(vm, one, d1, kNow, true);

  vm.screen = ui::Screen::MessageDetail;
  vm.messageIndex = 0;
  vm.messagePage = 0;
  int pages = ui::messagePageCount(renderer.sprite(), vm);
  render(renderer, full, vm, "12_message_page1");
  if (pages > 1) {
    vm.messagePage = 1;
    render(renderer, full, vm, "13_message_page2");
  }
  vm.messagePage = 0;

  // setup & system screens
  vm.screen = ui::Screen::SetupAP;
  strcpy(vm.ip, "192.168.4.1");
  render(renderer, full, vm, "20_setup_hotspot");
  strcpy(vm.ip, "192.168.178.47");
  vm.screen = ui::Screen::Connecting;
  vm.animPhase = 1;
  render(renderer, full, vm, "21_connecting");
  vm.screen = ui::Screen::NeedStops;
  render(renderer, full, vm, "22_need_stops");
  vm.screen = ui::Screen::SettingsQR;
  render(renderer, full, vm, "23_settings_qr");
  vm.screen = ui::Screen::ResetConfirm;
  render(renderer, full, vm, "28_reset_confirm");
  vm.screen = ui::Screen::NoWifi;
  render(renderer, full, vm, "24_no_wifi");
  vm.screen = ui::Screen::ResetHold;
  vm.resetSeconds = 4;
  vm.resetReleaseToCalibrate = true;
  render(renderer, full, vm, "25_reset_hold");
  vm.resetSeconds = 0;
  vm.resetReleaseToCalibrate = false;
  vm.resetReleaseToErase = true;
  render(renderer, full, vm, "25b_reset_release_erase");
  vm.resetReleaseToErase = false;
  vm.screen = ui::Screen::CalibrateIntro;
  render(renderer, full, vm, "26_calibrate_intro");
  vm.screen = ui::Screen::Boot;
  render(renderer, full, vm, "27_boot");

  // icon sheet
  {
    renderer.invalidate();
    renderer.render([&](ui::Painter& p) {
      p.clear(ui::theme::kBg);
      const WeatherIcon icons[] = {WeatherIcon::Clear,   WeatherIcon::MostlyClear,
                                   WeatherIcon::PartlyCloudy, WeatherIcon::Overcast,
                                   WeatherIcon::Fog,     WeatherIcon::Drizzle,
                                   WeatherIcon::Rain,    WeatherIcon::Showers,
                                   WeatherIcon::Snow,    WeatherIcon::Thunder};
      for (int i = 0; i < 10; i++) {
        int x = 32 + (i % 5) * 64, y = 36 + (i / 5) * 60;
        ui::icons::weather(p, icons[i], true, x, y, 50, ui::theme::kBg);
        ui::icons::weather(p, icons[i], false, x, y + 120, 50, ui::theme::kBg);
      }
    });
    savePng(full, "90_icon_sheet");
  }
  printf("done\n");
  return 0;
}
