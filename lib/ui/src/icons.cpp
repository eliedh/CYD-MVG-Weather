#include "icons.h"

#include <math.h>

#include "fonts.h"
#include "theme.h"

namespace ui {
namespace icons {

using namespace theme;

namespace {

inline int R(float v) { return (int)lroundf(v); }

// Cloud silhouette: three puffs on a rounded base. `k` = size/100,
// `grow` inflates every part (used to draw a background outline first).
void cloud(Painter& p, float cx, float cy, float k, uint32_t color, float grow = 0) {
  p.fillCircle(R(cx + 4 * k), R(cy - 6 * k), R(22 * k + grow), color);
  p.fillCircle(R(cx - 18 * k), R(cy + 6 * k), R(15 * k + grow), color);
  p.fillCircle(R(cx + 24 * k), R(cy + 6 * k), R(14 * k + grow), color);
  float x0 = cx - 33 * k - grow, x1 = cx + 38 * k + grow;
  float y0 = cy + 2 * k - grow, y1 = cy + 21 * k + grow;
  p.fillRoundRect(R(x0), R(y0), R(x1 - x0), R(y1 - y0), R((y1 - y0) / 2), color);
}

void cloudWithOutline(Painter& p, float cx, float cy, float k, uint32_t color, uint32_t bg) {
  cloud(p, cx, cy, k, bg, fmaxf(2.0f, 5 * k));
  cloud(p, cx, cy, k, color);
}

void sun(Painter& p, float cx, float cy, float k) {
  for (int i = 0; i < 8; i++) {
    float a = i * (float)M_PI / 4.0f;
    float c = cosf(a), s = sinf(a);
    p.line(R(cx + c * 26 * k), R(cy + s * 26 * k), R(cx + c * 35 * k), R(cy + s * 35 * k),
           fmaxf(1.0f, 3.2f * k), kSun);
  }
  p.fillCircle(R(cx), R(cy), R(18 * k), kSun);
}

void moon(Painter& p, float cx, float cy, float k, uint32_t bg) {
  p.fillCircle(R(cx), R(cy), R(25 * k), kMoon);
  p.fillCircle(R(cx + 13 * k), R(cy - 10 * k), R(21 * k), bg);
}

void sky(Painter& p, bool isDay, float cx, float cy, float k, uint32_t bg) {
  if (isDay) sun(p, cx, cy, k);
  else moon(p, cx, cy, k, bg);
}

void drops(Painter& p, float cx, float y, float k, int n, bool dots) {
  float xs[3] = {cx - 16 * k, cx + 1 * k, cx + 18 * k};
  for (int i = 0; i < n && i < 3; i++) {
    float yy = y + ((i % 2) ? 6 * k : 0);
    if (dots) {
      p.fillCircle(R(xs[i] - 2 * k), R(yy + 4 * k), R(fmaxf(1.5f, 3.2f * k)), kRain);
    } else {
      p.line(R(xs[i]), R(yy), R(xs[i] - 5 * k), R(yy + 13 * k), fmaxf(1.0f, 3.0f * k), kRain);
    }
  }
}

}  // namespace

void weather(Painter& p, core::WeatherIcon icon, bool isDay, int icx, int icy, int size,
             uint32_t bg) {
  using core::WeatherIcon;
  float k = size / 100.0f;
  float cx = icx, cy = icy;
  switch (icon) {
    case WeatherIcon::Clear:
      sky(p, isDay, cx, cy, k * 1.15f, bg);
      break;
    case WeatherIcon::MostlyClear:
      sky(p, isDay, cx - 8 * k, cy - 8 * k, k, bg);
      cloudWithOutline(p, cx + 16 * k, cy + 22 * k, k * 0.55f, kCloud, bg);
      break;
    case WeatherIcon::PartlyCloudy:
      sky(p, isDay, cx - 14 * k, cy - 14 * k, k * 0.82f, bg);
      cloudWithOutline(p, cx + 4 * k, cy + 12 * k, k * 0.88f, kCloud, bg);
      break;
    case WeatherIcon::Overcast:
      cloud(p, cx + 14 * k, cy - 12 * k, k * 0.72f, kCloudDark);
      cloudWithOutline(p, cx - 4 * k, cy + 8 * k, k * 0.92f, kCloud, bg);
      break;
    case WeatherIcon::Fog:
      cloud(p, cx, cy - 12 * k, k * 0.85f, kCloudDark);
      p.line(R(cx - 30 * k), R(cy + 22 * k), R(cx + 26 * k), R(cy + 22 * k), fmaxf(1, 3.2f * k), kCloud);
      p.line(R(cx - 22 * k), R(cy + 34 * k), R(cx + 34 * k), R(cy + 34 * k), fmaxf(1, 3.2f * k), kCloud);
      break;
    case WeatherIcon::Drizzle:
      cloud(p, cx, cy - 10 * k, k * 0.9f, kCloud);
      drops(p, cx, cy + 22 * k, k, 3, true);
      break;
    case WeatherIcon::Rain:
      cloud(p, cx, cy - 10 * k, k * 0.9f, kCloud);
      drops(p, cx, cy + 20 * k, k, 3, false);
      break;
    case WeatherIcon::Showers:
      sky(p, isDay, cx - 16 * k, cy - 18 * k, k * 0.7f, bg);
      cloudWithOutline(p, cx + 2 * k, cy - 4 * k, k * 0.85f, kCloud, bg);
      drops(p, cx + 2 * k, cy + 24 * k, k * 0.9f, 3, false);
      break;
    case WeatherIcon::Snow: {
      cloud(p, cx, cy - 10 * k, k * 0.9f, kCloud);
      float r = fmaxf(1.5f, 4.0f * k);
      p.fillCircle(R(cx - 16 * k), R(cy + 24 * k), R(r), kSnow);
      p.fillCircle(R(cx + 1 * k), R(cy + 32 * k), R(r), kSnow);
      p.fillCircle(R(cx + 18 * k), R(cy + 24 * k), R(r), kSnow);
      break;
    }
    case WeatherIcon::Thunder:
      cloud(p, cx, cy - 12 * k, k * 0.9f, kCloudDark);
      p.triangle(R(cx + 6 * k), R(cy + 8 * k), R(cx - 12 * k), R(cy + 30 * k), R(cx + 2 * k),
                 R(cy + 28 * k), kBolt);
      p.triangle(R(cx - 2 * k), R(cy + 24 * k), R(cx + 12 * k), R(cy + 24 * k), R(cx - 8 * k),
                 R(cy + 46 * k), kBolt);
      break;
    default:
      cloud(p, cx, cy, k * 0.9f, kCloudDark);
      break;
  }
}

void drop(Painter& p, int cx, int cy, int r, uint32_t color) {
  // teardrop: circle + triangle pointing up
  p.fillCircle(cx, cy + r / 2, r, color);
  p.triangle(cx - r + 1, cy + r / 2 - 1, cx + r - 1, cy + r / 2 - 1, cx, cy - r - r / 2, color);
}

void clock(Painter& p, int cx, int cy, int r, uint32_t color, uint32_t bg) {
  p.fillCircle(cx, cy, r, color);
  p.fillCircle(cx, cy, r - 2, bg);
  p.line(cx, cy, cx, cy - r + 3, 0.8f, color);
  p.line(cx, cy, cx + r - 4, cy, 0.8f, color);
}

void warning(Painter& p, int x, int y, int s, uint32_t color, uint32_t ink) {
  // Rounded triangle: a filled triangle plus thick AA edges for soft corners.
  float hw = fmaxf(1.0f, s / 10.0f);
  int ax = x + s / 2, ay = y + (int)hw;
  int bx = x + (int)hw, by = y + s - (int)hw;
  int cx = x + s - (int)hw, cy = by;
  p.triangle(ax, ay, bx, by, cx, cy, color);
  p.line(ax, ay, bx, by, hw, color);
  p.line(bx, by, cx, cy, hw, color);
  p.line(cx, cy, ax, ay, hw, color);
  float iw = fmaxf(0.9f, s / 15.0f);
  p.line(ax, y + s * 38 / 100, ax, y + s * 62 / 100, iw, ink);
  p.fillCircle(ax, y + s * 79 / 100, (int)lroundf(iw * 1.1f), ink);
}

static void arcStroke(Painter& p, int cx, int cy, float r, float a0, float a1, float hw,
                      uint32_t color) {
  const float step = 10.0f;
  float a = a0;
  float px = cx + r * cosf(a * (float)M_PI / 180), py = cy + r * sinf(a * (float)M_PI / 180);
  while (a < a1) {
    a = fminf(a + step, a1);
    float nx = cx + r * cosf(a * (float)M_PI / 180), ny = cy + r * sinf(a * (float)M_PI / 180);
    p.line(R(px), R(py), R(nx), R(ny), hw, color);
    px = nx;
    py = ny;
  }
}

void wifi(Painter& p, int cx, int cy, int s, uint32_t color, bool crossed, uint32_t bg) {
  // three arcs + dot opening upwards; (cx, cy) is the dot centre
  float hw = fmaxf(0.9f, s / 14.0f);
  for (int i = 1; i <= 3; i++) arcStroke(p, cx, cy, i * s / 3.0f, 225, 315, hw, color);
  p.fillCircle(cx, cy, (int)lroundf(hw * 1.4f), color);
  if (crossed) {
    int x0 = cx - s * 2 / 3, y0 = cy - s + 2, x1 = cx + s * 2 / 3, y1 = cy + 4;
    p.line(x0, y0, x1, y1, hw + 2.5f, bg);
    p.line(x0, y0, x1, y1, hw, kDanger);
  }
}

void chevronRight(Painter& p, int cx, int cy, int s, uint32_t color) {
  p.line(cx - s / 4, cy - s / 2, cx + s / 4, cy, 1.1f, color);
  p.line(cx + s / 4, cy, cx - s / 4, cy + s / 2, 1.1f, color);
}

void liveDot(Painter& p, int cx, int cy, uint32_t color) { p.fillCircle(cx, cy, 3, color); }

void logo(Painter& p, int x, int y, int s) {
  // A departure-board tile: rounded square with two "rows" and a clock hand.
  p.fillRoundRect(x, y, s, s, s / 4, kAccent);
  int m = s / 5;
  p.fillRoundRect(x + m, y + m + 1, s - 2 * m, s / 7 + 1, s / 14 + 1, kAccentInk);
  p.fillRoundRect(x + m, y + s / 2 - 1, s * 2 / 5, s / 7 + 1, s / 14 + 1, kAccentInk);
  p.fillRoundRect(x + m, y + s - m - s / 7, s / 2 + 2, s / 7 + 1, s / 14 + 1, kAccentInk);
}

}  // namespace icons
}  // namespace ui
