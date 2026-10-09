#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "fonts.h"
#include "i18n.h"
#include "icons.h"
#include "theme.h"
#include "widgets.h"

namespace ui {

using namespace theme;
using core::Lang;
using core::Str;
using core::tr;

namespace {

const char* T(const ViewModel& vm, Str k) { return tr(k, vm.lang); }

int roundTemp(float t) {
  int v = (int)lroundf(t);
  return v == 0 ? 0 : v;  // avoid "-0"
}

void fmtTemp(char* buf, size_t n, float t) { snprintf(buf, n, "%d\xC2\xB0", roundTemp(t)); }

void centeredText(Painter& p, const char* s, int baseline, const lgfx::IFont* f, uint32_t c) {
  p.text(s, kW / 2, baseline, f, c, textdatum_t::baseline_center);
}

void centeredWrapped(Painter& p, const char* s, int baseline, int lineH, const lgfx::IFont* f,
                     uint32_t c, int maxW = kW - 2 * 24) {
  for (const auto& l : p.wrap(s, f, maxW)) {
    centeredText(p, l.c_str(), baseline, f, c);
    baseline += lineH;
  }
}

core::TransportType guessType(const ViewModel& vm, const char* label) {
  for (int i = 0; i < vm.rowCount; i++)
    if (strcmp(vm.rows[i].dep.label, label) == 0) return vm.rows[i].dep.type;
  if (label[0] == 'U' && label[1] >= '0' && label[1] <= '9') return core::TransportType::UBahn;
  if (label[0] == 'S' && label[1] >= '0' && label[1] <= '9') return core::TransportType::SBahn;
  return core::TransportType::Bus;
}

// ------------------------------------------------------------------ header

void drawStatusLine(Painter& p, const ViewModel& vm, int right, int baseline) {
  char buf[48];
  int stale = staleMinutes(vm);
  if (vm.wifiDown) {
    int w = p.textWidth(T(vm, Str::WifiLost), fonts::small());
    p.text(T(vm, Str::WifiLost), right, baseline, fonts::small(), kWarn, textdatum_t::baseline_right);
    icons::wifi(p, right - w - 9, baseline - 1, 9, kWarn, false, kBg);
  } else if (stale >= 0) {
    if (stale >= 120) snprintf(buf, sizeof(buf), T(vm, Str::StaleHour), stale / 60);
    else snprintf(buf, sizeof(buf), T(vm, Str::StaleMin), stale);
    int w = p.textWidth(buf, fonts::small());
    p.text(buf, right, baseline, fonts::small(), kWarn, textdatum_t::baseline_right);
    icons::clock(p, right - w - 9, baseline - 5, 5, kWarn, kBg);
  } else if (vm.timeValid) {
    core::formatDate(buf, sizeof(buf), vm.local, vm.lang);
    p.text(buf, right, baseline, fonts::small(), kText2, textdatum_t::baseline_right);
  }
}

void drawHeader(Painter& p, const ViewModel& vm) {
  if (!p.touches(0, kHeaderH)) return;
  const core::WeatherData& w = vm.weather;
  char buf[24];
  int x = 62;
  if (w.valid) {
    icons::weather(p, core::weatherIconForCode(w.code), w.isDay, 31, 29, 46, kBg);
    fmtTemp(buf, sizeof(buf), w.temp);
    x += p.text(buf, x, 46, fonts::big(), kText) + 10;
    char hl[32];
    snprintf(hl, sizeof(hl), "\xE2\x86\x91%d\xC2\xB0  \xE2\x86\x93%d\xC2\xB0", roundTemp(w.tMax),
             roundTemp(w.tMin));
    p.text(hl, x, 25, fonts::small(), kText2);
    icons::drop(p, x + 4, 39, 4, kInfo);
    snprintf(buf, sizeof(buf), "%d %%", w.rainNextHours);
    p.text(buf, x + 13, 46, fonts::small(), kText2);
  } else {
    icons::weather(p, core::WeatherIcon::Unknown, true, 31, 29, 46, kBg);
    p.text("--\xC2\xB0", x, 46, fonts::big(), kText3);
  }
  if (vm.timeValid) {
    snprintf(buf, sizeof(buf), "%02d:%02d", vm.local.tm_hour, vm.local.tm_min);
    p.text(buf, kW - kPad, 28, fonts::title(), kText, textdatum_t::baseline_right);
  }
  drawStatusLine(p, vm, kW - kPad, 46);
  p.hline(kPad, kHeaderH - 1, kW - 2 * kPad, kDivider);
}

// ------------------------------------------------------------------ banner

void drawBanner(Painter& p, const ViewModel& vm, int y) {
  if (!p.touches(y, kBannerH)) return;
  const core::ServiceMessage& m = vm.messages[0];
  p.fillRoundRect(8, y, kW - 16, kBannerH, 7, kSurface);
  icons::warning(p, 14, y + 5, 14, kWarn, kWarnInk);
  int x = 36;
  int right = kW - 22;
  if (vm.messageCount > 1) {
    char more[16];
    snprintf(more, sizeof(more), "+%d", vm.messageCount - 1);
    right -= p.text(more, right, y + 17, fonts::small(), kText3, textdatum_t::baseline_right) + 6;
  }
  x += p.text(m.lines, x, y + 17, fonts::small(), kWarn) + 6;
  std::string t = p.ellipsize(m.title, fonts::small(), right - x);
  p.text(t.c_str(), x, y + 17, fonts::small(), kText);
  icons::chevronRight(p, kW - 16, y + kBannerH / 2, 8, kText3);
}

// ------------------------------------------------------------------ rows

void drawRow(Painter& p, const ViewModel& vm, const core::BoardRow& r, int y) {
  if (!p.touches(y, kRowH)) return;
  const core::Departure& d = r.dep;
  if (r.highlight) p.fillRoundRect(4, y + 1, kW - 8, kRowH - 2, kRadius, kSurfaceHi);

  // Unreachable (walking time too long): everything in muted colours.
  const bool missed = !r.reachable;
  widgets::lineBadge(p, 12, y + (kRowH - kBadgeH) / 2, kBadgeW, kBadgeH, d.type, d.label,
                     d.cancelled || missed);

  // ---- right column: minutes / time / cancelled
  char num[12] = {0};
  const char* unit = nullptr;
  const lgfx::IFont* numFont = fonts::title();
  uint32_t numColor = missed ? kMissed : (d.realtime ? kText : kText2);
  int base = y + 24;
  if (d.cancelled) {
    snprintf(num, sizeof(num), "%s", "");
  } else if (r.minutes <= 0) {
    snprintf(num, sizeof(num), "%s", T(vm, Str::Now));
    numFont = fonts::body();
    numColor = missed ? kMissed : (d.realtime ? kAccent : kText);
  } else if (r.minutes >= 60) {
    time_t t = (time_t)d.effectiveTime();
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(num, sizeof(num), "%02d:%02d", lt.tm_hour, lt.tm_min);
    numFont = fonts::body();
  } else {
    snprintf(num, sizeof(num), "%d", r.minutes);
    unit = T(vm, Str::MinShort);
  }

  int right = kW - 12;
  int colLeft;
  if (d.cancelled) {
    colLeft = right - p.text(T(vm, Str::Cancelled), right, y + 22, fonts::small(), kDanger,
                             textdatum_t::baseline_right);
  } else {
    int uw = unit ? p.textWidth(unit, fonts::small()) + 3 : 0;
    if (unit)
      p.text(unit, right, base, fonts::small(), missed ? kMissed : kText2,
             textdatum_t::baseline_right);
    int nw = p.text(num, right - uw, base, numFont, numColor, textdatum_t::baseline_right);
    colLeft = right - uw - nw;
    if (d.realtime) {
      icons::liveDot(p, colLeft - 7, base - 13, missed ? kMissed : kAccent);
      colLeft -= 10;
    }
    if (d.realtime && d.delayMin > 0) {
      char dl[8];
      snprintf(dl, sizeof(dl), "+%d", d.delayMin);
      colLeft -= p.text(dl, colLeft - 3, base, fonts::small(), missed ? kMissed : kWarn,
                        textdatum_t::baseline_right) + 3;
    }
  }

  // ---- middle: destination + optional sub line
  int tx = 12 + kBadgeW + 10;
  int maxW = colLeft - 10 - tx;
  char sub[64] = {0};
  bool subAccent = false;
  if (r.highlight && r.leaveInMin >= 0) {
    if (r.leaveInMin == 0) snprintf(sub, sizeof(sub), "%s", T(vm, Str::LeaveNow));
    else snprintf(sub, sizeof(sub), T(vm, Str::LeaveIn), r.leaveInMin);
    subAccent = true;
  }
  const char* stopLabel = vm.stopCount > 1 ? vm.stopLabels[d.stopIndex % core::kMaxStops] : "";
  bool twoLines = sub[0] || stopLabel[0];
  int destBase = twoLines ? y + 16 : y + 23;
  std::string dest = p.ellipsize(d.destination, fonts::body(), maxW);
  uint32_t destColor = d.cancelled ? kText3 : (missed ? kMissed : kText);
  int dw = p.text(dest.c_str(), tx, destBase, fonts::body(), destColor);
  if (d.cancelled) p.line(tx, destBase - 5, tx + dw, destBase - 5, 0.9f, kText2);

  if (twoLines) {
    int sx = tx;
    int subBase = y + 30;
    int limit = tx + maxW;
    if (sub[0]) {
      std::string s = p.ellipsize(sub, fonts::small(), limit - sx);
      sx += p.text(s.c_str(), sx, subBase, fonts::small(), subAccent ? kAccent : kText2);
      if (stopLabel[0]) sx += p.text("  \xC2\xB7  ", sx, subBase, fonts::small(), kText3);
    }
    if (stopLabel[0] && limit - sx > 24) {
      std::string sl = p.ellipsize(stopLabel, fonts::small(), limit - sx);
      p.text(sl.c_str(), sx, subBase, fonts::small(), kText3);
    }
  }
}

void drawMain(Painter& p, const ViewModel& vm) {
  drawHeader(p, vm);
  bool banner = mainHasBanner(vm);
  int y = kHeaderH + 4;
  if (banner) {
    drawBanner(p, vm, y);
    y += kBannerH + 4;
  }
  int visible = mainVisibleRows(vm);
  int pages = mainPageCount(vm);
  int page = vm.boardPage < pages ? vm.boardPage : pages - 1;
  int first = page * visible;
  int n = vm.rowCount - first;
  if (n > visible) n = visible;
  if (n < 0) n = 0;
  for (int i = 0; i < n; i++) drawRow(p, vm, vm.rows[first + i], y + i * kRowH);

  // Page dots in the bottom margin, only when there is more than one page.
  if (pages > 1) {
    const int gap = 10, dy = kH - 4;
    int x0 = kW / 2 - (pages - 1) * gap / 2;
    for (int i = 0; i < pages; i++)
      p.fillCircle(x0 + i * gap, dy, i == page ? 3 : 2, i == page ? kText2 : kDivider);
  }

  if (n == 0) {
    int cy = (y + kH) / 2;
    if (!vm.hasDepartureData && vm.departuresError) {
      centeredText(p, T(vm, Str::ApiError), cy, fonts::body(), kText2);
    } else if (!vm.hasDepartureData) {
      centeredText(p, T(vm, Str::Loading), cy - 6, fonts::body(), kText2);
      widgets::dots(p, kW / 2, cy + 16, vm.animPhase, kText2);
    } else {
      centeredText(p, T(vm, Str::NoDepartures), cy, fonts::body(), kText2);
    }
  }
}

// ------------------------------------------------------------------ weather detail

void drawWeatherDetail(Painter& p, const ViewModel& vm) {
  const core::WeatherData& w = vm.weather;
  char buf[48];
  if (!w.valid) {
    icons::weather(p, core::WeatherIcon::Unknown, true, kW / 2, 96, 80, kBg);
    centeredText(p, T(vm, Str::WeatherNA), 168, fonts::body(), kText2);
    centeredText(p, T(vm, Str::TapToClose), 228, fonts::small(), kText3);
    return;
  }
  icons::weather(p, core::weatherIconForCode(w.code), w.isDay, 46, 42, 70, kBg);
  fmtTemp(buf, sizeof(buf), w.temp);
  int tx = 92;
  int tw = p.text(buf, tx, 58, fonts::big(), kText);
  // high / low / rain, stacked on the right
  int rx = tx + tw + 18;
  snprintf(buf, sizeof(buf), "\xE2\x86\x91 %d\xC2\xB0", roundTemp(w.tMax));
  p.text(buf, rx, 26, fonts::body(), kText);
  snprintf(buf, sizeof(buf), "\xE2\x86\x93 %d\xC2\xB0", roundTemp(w.tMin));
  p.text(buf, rx, 48, fonts::body(), kText2);
  if (vm.timeValid) {
    snprintf(buf, sizeof(buf), "%02d:%02d", vm.local.tm_hour, vm.local.tm_min);
    p.text(buf, kW - kPad, 26, fonts::body(), kText2, textdatum_t::baseline_right);
  }
  snprintf(buf, sizeof(buf), "%d %%", w.precipMaxToday);
  int rw = p.text(buf, kW - kPad, 48, fonts::body(), kText2, textdatum_t::baseline_right);
  icons::drop(p, kW - kPad - rw - 9, 41, 4, kInfo);

  // description line
  const char* desc = core::weatherText(w.code, vm.lang);
  int dx = kPad;
  dx += p.text(desc, dx, 98, fonts::body(), kText);
  char extra[64];
  char feels[24], wind[24];
  snprintf(feels, sizeof(feels), T(vm, Str::FeelsLike), roundTemp(w.feelsLike));
  snprintf(wind, sizeof(wind), T(vm, Str::Wind), (int)lroundf(w.windKmh));
  snprintf(extra, sizeof(extra), "  \xC2\xB7  %s  \xC2\xB7  %s", feels, wind);
  std::string e = p.ellipsize(extra, fonts::small(), kW - kPad - dx);
  p.text(e.c_str(), dx, 98, fonts::small(), kText2);

  p.hline(kPad, 110, kW - 2 * kPad, kDivider);
  p.text(T(vm, Str::NextHours), kPad, 130, fonts::small(), kText3);

  int n = w.nHours < 8 ? w.nHours : 8;
  int colW = (kW - 2 * kPad) / 8;
  for (int i = 0; i < n; i++) {
    const core::HourForecast& h = w.hours[i];
    int cx = kPad + colW * i + colW / 2;
    if (i == 0) {
      snprintf(buf, sizeof(buf), "%s", T(vm, Str::NowCap));
    } else {
      time_t t = (time_t)h.time;
      struct tm lt;
      localtime_r(&t, &lt);
      snprintf(buf, sizeof(buf), "%02d", lt.tm_hour);
    }
    p.text(buf, cx, 152, fonts::small(), i == 0 ? kText : kText2, textdatum_t::baseline_center);
    icons::weather(p, core::weatherIconForCode(h.code), h.isDay, cx, 174, 30, kBg);
    fmtTemp(buf, sizeof(buf), h.temp);
    p.text(buf, cx, 210, fonts::body(), kText, textdatum_t::baseline_center);
    snprintf(buf, sizeof(buf), "%d%%", h.precipProb);
    p.text(buf, cx, 229, fonts::small(), h.precipProb >= 30 ? kInfo : kText3,
           textdatum_t::baseline_center);
  }
}

// ------------------------------------------------------------------ message detail

struct MsgLayout {
  int key = -1;
  Lang lang = Lang::De;
  std::string titleRef;
  std::vector<std::string> title;
  std::vector<std::vector<std::string>> pages;
};
MsgLayout g_msgLayout;

constexpr int kMsgTop = 66, kMsgBottom = 208, kMsgTitleLH = 21, kMsgLH = 17;

const MsgLayout& layoutMessage(lgfx::LovyanGFX& g, const ViewModel& vm) {
  const core::ServiceMessage& m = vm.messages[vm.messageIndex % core::kMaxMessages];
  if (g_msgLayout.key == vm.messageIndex && g_msgLayout.lang == vm.lang &&
      g_msgLayout.titleRef == m.title)
    return g_msgLayout;
  Painter measure(g, 0, kH);
  MsgLayout L;
  L.key = vm.messageIndex;
  L.lang = vm.lang;
  L.titleRef = m.title;
  int maxW = kW - 2 * kPad - 4;
  L.title = measure.wrap(m.title, fonts::body(), maxW);
  if (L.title.size() > 3) L.title.resize(3);
  std::vector<std::string> body = measure.wrap(m.text, fonts::small(), maxW);
  int first = kMsgTop + (int)L.title.size() * kMsgTitleLH + 6;
  int cap0 = first > kMsgBottom ? 0 : (kMsgBottom - first) / kMsgLH + 1;
  int capN = (kMsgBottom - kMsgTop) / kMsgLH + 1;
  size_t i = 0;
  L.pages.emplace_back();
  while (i < body.size() && (int)L.pages[0].size() < cap0) L.pages[0].push_back(body[i++]);
  while (i < body.size()) {
    L.pages.emplace_back();
    while (i < body.size() && (int)L.pages.back().size() < capN) L.pages.back().push_back(body[i++]);
  }
  g_msgLayout = L;
  return g_msgLayout;
}

void drawMessageDetail(Painter& p, const ViewModel& vm) {
  if (vm.messageCount == 0 || !vm.messages) return;
  const core::ServiceMessage& m = vm.messages[vm.messageIndex % core::kMaxMessages];
  const MsgLayout& L = layoutMessage(p.gfx(), vm);
  int pages = (int)L.pages.size();
  int page = vm.messagePage < pages ? vm.messagePage : pages - 1;

  // header: warning + "Service notice" + line badges
  icons::warning(p, kPad, 12, 22, kWarn, kWarnInk);
  p.text(T(vm, Str::Notice), 42, 29, fonts::body(), kText2);
  int bx = kW - kPad;
  // badges right-aligned, parsed from "U3, U6"
  std::vector<std::string> labels;
  {
    std::string s = m.lines, cur;
    for (char c : s) {
      if (c == ',' || c == ' ') {
        if (!cur.empty()) labels.push_back(cur);
        cur.clear();
      } else {
        cur += c;
      }
    }
    if (!cur.empty()) labels.push_back(cur);
  }
  for (int i = (int)labels.size() - 1; i >= 0 && bx > 200; i--) {
    int w = p.textWidth(labels[i].c_str(), fonts::badge()) + 10;
    if (w < 34) w = 34;
    bx -= w;
    widgets::lineBadge(p, bx, 12, w, 20, guessType(vm, labels[i].c_str()), labels[i].c_str());
    bx -= 6;
  }
  p.hline(kPad, 44, kW - 2 * kPad, kDivider);

  int y = kMsgTop;
  if (page == 0) {
    y = widgets::lines(p, L.title, kPad, y, kMsgTitleLH, fonts::body(), kText) - kMsgTitleLH + 6 +
        kMsgLH;
  }
  widgets::lines(p, L.pages[page], kPad, y, kMsgLH, fonts::small(), kText2);

  // footer
  bool last = page >= pages - 1;
  bool moreMsgs = vm.messageIndex + 1 < vm.messageCount;
  p.text(T(vm, (last && !moreMsgs) ? Str::TapToClose : Str::TapNext), kPad, 232, fonts::small(),
         kText3);
  if (pages > 1 || vm.messageCount > 1) {
    char pg[48];
    if (vm.messageCount > 1)
      snprintf(pg, sizeof(pg), "%d/%d  \xC2\xB7  %d/%d", vm.messageIndex + 1, vm.messageCount,
               page + 1, pages);
    else snprintf(pg, sizeof(pg), T(vm, Str::PageOf), page + 1, pages);
    p.text(pg, kW - kPad, 232, fonts::small(), kText3, textdatum_t::baseline_right);
  }
}

// ------------------------------------------------------------------ setup / QR screens

void titleRow(Painter& p, const char* title, const char* secondary) {
  icons::logo(p, kPad, 11, 24);
  int x = kPad + 34;
  x += p.text(title, x, 31, fonts::title(), kText);
  if (secondary) p.text(secondary, x + 8, 31, fonts::title(), kText3);
}

void drawSetupAP(Painter& p, const ViewModel& vm) {
  titleRow(p, tr(Str::WelcomeTitle, Lang::De), tr(Str::WelcomeTitle, Lang::En));
  char qr[96];
  snprintf(qr, sizeof(qr), "WIFI:T:nopass;S:%s;;", vm.apSsid);
  const int qx = kPad, qy = 48, qs = 120;
  widgets::qrCard(p, qr, qx, qy, qs);
  int qc = qx + qs / 2;
  std::string ssid = p.ellipsize(vm.apSsid, fonts::small(), qs + 16);
  p.text(ssid.c_str(), qc, qy + qs + 18, fonts::small(), kText, textdatum_t::baseline_center);
  p.text(tr(Str::SetupOpenNet, Lang::De), qc, qy + qs + 35, fonts::small(), kText3,
         textdatum_t::baseline_center);
  p.text(tr(Str::SetupOpenNet, Lang::En), qc, qy + qs + 50, fonts::small(), kText3,
         textdatum_t::baseline_center);

  const Str steps[3] = {Str::SetupStep1, Str::SetupStep2, Str::SetupStep3};
  int bx = qx + qs + 20;  // bullet centre
  int tx = bx + 18;
  int maxW = kW - kPad - tx + 4;
  int y = qy + 16;
  for (int i = 0; i < 3; i++) {
    widgets::stepBullet(p, bx, y - 5, i + 1);
    auto de = p.wrap(tr(steps[i], Lang::De), fonts::small(), maxW);
    auto en = p.wrap(tr(steps[i], Lang::En), fonts::small(), maxW);
    y = widgets::lines(p, de, tx, y, 15, fonts::small(), kText);
    y = widgets::lines(p, en, tx, y, 15, fonts::small(), kText3);
    y += 11;
  }
  char fb[48];
  snprintf(fb, sizeof(fb), "http://%s", vm.ip[0] ? vm.ip : "192.168.4.1");
  p.text(fb, kW - kPad, 232, fonts::small(), kText3, textdatum_t::baseline_right);
}

void drawUrlScreen(Painter& p, const ViewModel& vm, Str title, Str body, bool closeHint) {
  titleRow(p, T(vm, title), nullptr);
  char url[64];
  snprintf(url, sizeof(url), "http://%s/", vm.ip);
  const int qx = kPad, qy = 50, qs = 128;
  widgets::qrCard(p, url, qx, qy, qs);
  int tx = qx + qs + 16;
  int maxW = kW - kPad - tx;
  int y = qy + 16;
  y = widgets::lines(p, p.wrap(T(vm, body), fonts::small(), maxW), tx, y, 17, fonts::small(),
                     kText2);
  y += 14;
  char host[48];
  snprintf(host, sizeof(host), "%s", vm.host);
  std::string h = p.ellipsize(host, fonts::body(), maxW);
  p.text(h.c_str(), tx, y, fonts::body(), kAccent);
  y += 22;
  p.text(vm.ip, tx, y, fonts::body(), kText);
  y += 22;
  widgets::lines(p, p.wrap(T(vm, Str::AlmostSameWifi), fonts::small(), maxW), tx, y, 16,
                 fonts::small(), kText3);
  if (closeHint) {  // settings QR: reset button bottom-left, close hint right
    const int bx = kPad, by = 206, bw = 132, bh = 28;
    p.fillRoundRect(bx, by, bw, bh, 8, kSurface);
    p.text(T(vm, Str::ResetButton), bx + bw / 2, by + bh / 2 + 1, fonts::small(), kText2,
           textdatum_t::middle_center);
    p.text(T(vm, Str::TapToClose), kW - kPad, 225, fonts::small(), kText3,
           textdatum_t::baseline_right);
  }
}

constexpr int kBtnY = 164, kBtnH = 42, kBtnGap = 12;

void drawResetConfirm(Painter& p, const ViewModel& vm) {
  icons::warning(p, kW / 2 - 15, 16, 30, kDanger, kBg);
  centeredText(p, T(vm, Str::ResetAskTitle), 78, fonts::title(), kText);
  centeredWrapped(p, T(vm, Str::ResetAskBody), 104, 18, fonts::small(), kText2, kW - 40);
  int bw = (kW - 2 * kPad - kBtnGap) / 2;
  int x1 = kPad, x2 = kPad + bw + kBtnGap;
  p.fillRoundRect(x1, kBtnY, bw, kBtnH, 10, kSurfaceHi);
  p.text(T(vm, Str::Cancel), x1 + bw / 2, kBtnY + kBtnH / 2 + 1, fonts::body(), kText,
         textdatum_t::middle_center);
  p.fillRoundRect(x2, kBtnY, bw, kBtnH, 10, kDanger);
  p.text(T(vm, Str::Erase), x2 + bw / 2, kBtnY + kBtnH / 2 + 1, fonts::body(), 0x2A0A0A,
         textdatum_t::middle_center);
  centeredText(p, T(vm, Str::HoldToErase), 228, fonts::small(), kText3);
}

void drawConnecting(Painter& p, const ViewModel& vm) {
  icons::logo(p, kW / 2 - 22, 46, 44);
  centeredText(p, "Abfahrt", 128, fonts::title(), kText);
  centeredText(p, T(vm, Str::ConnectingTitle), 160, fonts::body(), kText2);
  if (vm.homeSsid[0]) {
    char b[64];
    snprintf(b, sizeof(b), T(vm, Str::ConnectingBody), vm.homeSsid);
    std::string s = p.ellipsize(b, fonts::small(), kW - 40);
    centeredText(p, s.c_str(), 182, fonts::small(), kText3);
  }
  widgets::dots(p, kW / 2, 212, vm.animPhase, kAccent);
}

void drawNoWifi(Painter& p, const ViewModel& vm) {
  icons::wifi(p, kW / 2, 92, 48, kText2, true, kBg);
  centeredText(p, T(vm, Str::NoWifiTitle), 136, fonts::title(), kText);
  char b[80];
  snprintf(b, sizeof(b), T(vm, Str::NoWifiBody), vm.homeSsid);
  centeredWrapped(p, b, 164, 18, fonts::small(), kText2);
  centeredText(p, T(vm, Str::NoWifiRetry), 184, fonts::small(), kText2);
  widgets::dots(p, kW / 2, 202, vm.animPhase, kText3);
  centeredText(p, T(vm, Str::NoWifiHint), 230, fonts::small(), kText3);
}

void drawBoot(Painter& p, const ViewModel& vm) {
  icons::logo(p, kW / 2 - 22, 60, 44);
  centeredText(p, "Abfahrt", 142, fonts::title(), kText);
  centeredText(p, T(vm, Str::Starting), 170, fonts::small(), kText3);
}

void drawResetHold(Painter& p, const ViewModel& vm) {
  char b[64];
  if (vm.resetReleaseToErase) {
    icons::warning(p, kW / 2 - 22, 60, 44, kDanger, kBg);
    centeredWrapped(p, T(vm, Str::ResetReleaseErase), 146, 28, fonts::title(), kDanger, kW - 60);
    return;
  }
  snprintf(b, sizeof(b), "%d", vm.resetSeconds);
  centeredText(p, b, 116, fonts::big(), kWarn);
  snprintf(b, sizeof(b), T(vm, Str::ResetHold), vm.resetSeconds);
  centeredText(p, b, 156, fonts::body(), kText);
  if (vm.resetReleaseToCalibrate)
    centeredText(p, T(vm, Str::ResetRelease), 190, fonts::small(), kAccent);
}

void drawCalibrateIntro(Painter& p, const ViewModel& vm) {
  centeredText(p, T(vm, Str::CalTitle), 104, fonts::title(), kText);
  centeredWrapped(p, T(vm, Str::CalBody), 136, 18, fonts::small(), kText2, 240);
}

void drawNotice(Painter& p, const ViewModel& vm) {
  if (vm.noticeTitle) centeredText(p, vm.noticeTitle, 118, fonts::title(), kText);
  if (vm.noticeBody) centeredWrapped(p, vm.noticeBody, 148, 18, fonts::small(), kText2);
}

}  // namespace

int staleMinutes(const ViewModel& vm) {
  if (!vm.hasDepartureData || !vm.departuresFetchedAt || !vm.now) return -1;
  int64_t age = (vm.now - vm.departuresFetchedAt) / 60;
  if (age < 0) return -1;
  if ((vm.departuresError && age >= 1) || age >= 3) return (int)age;
  return -1;
}

bool mainHasBanner(const ViewModel& vm) { return vm.messageCount > 0 && vm.messages; }

int mainPageCount(const ViewModel& vm) {
  int visible = mainVisibleRows(vm);
  if (visible <= 0 || vm.rowCount <= visible) return 1;
  int pages = (vm.rowCount + visible - 1) / visible;
  return pages > kMaxBoardPages ? kMaxBoardPages : pages;
}

int mainVisibleRows(const ViewModel& vm) {
  int top = kHeaderH + 4 + (mainHasBanner(vm) ? kBannerH + 4 : 0);
  int n = (kH - top) / kRowH;
  return n > kMaxBoardRows ? kMaxBoardRows : n;
}

HitZone hitTestMain(const ViewModel& vm, int x, int y) {
  (void)x;
  if (y < kHeaderH) return HitZone::Header;
  int by = kHeaderH + 4;
  // generous touch target: +-6 px around the banner
  if (mainHasBanner(vm) && y >= by - 4 && y < by + kBannerH + 6) return HitZone::Banner;
  return HitZone::Body;
}

HitZone hitTestSettingsQR(int x, int y) {
  // generous target around the 132x28 button at (10, 206)
  return (x < 160 && y >= 196) ? HitZone::ResetButton : HitZone::None;
}

HitZone hitTestResetConfirm(int x, int y) {
  if (y < kBtnY - 10 || y > kBtnY + kBtnH + 10) return HitZone::None;
  return x < kW / 2 ? HitZone::Cancel : HitZone::Erase;
}

int messagePageCount(lgfx::LovyanGFX& measure, const ViewModel& vm) {
  if (vm.messageCount == 0 || !vm.messages) return 0;
  return (int)layoutMessage(measure, vm).pages.size();
}

void drawScreen(Painter& p, const ViewModel& vm) {
  p.clear(kBg);
  switch (vm.screen) {
    case Screen::Boot: drawBoot(p, vm); break;
    case Screen::SetupAP: drawSetupAP(p, vm); break;
    case Screen::Connecting: drawConnecting(p, vm); break;
    case Screen::NoWifi: drawNoWifi(p, vm); break;
    case Screen::NeedStops: drawUrlScreen(p, vm, Str::AlmostTitle, Str::AlmostBody, false); break;
    case Screen::Main: drawMain(p, vm); break;
    case Screen::WeatherDetail: drawWeatherDetail(p, vm); break;
    case Screen::MessageDetail: drawMessageDetail(p, vm); break;
    case Screen::SettingsQR: drawUrlScreen(p, vm, Str::SettingsTitle, Str::SettingsBody, true); break;
    case Screen::ResetConfirm: drawResetConfirm(p, vm); break;
    case Screen::ResetHold: drawResetHold(p, vm); break;
    case Screen::CalibrateIntro: drawCalibrateIntro(p, vm); break;
    case Screen::Notice: drawNotice(p, vm); break;
  }
}

}  // namespace ui
