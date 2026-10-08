// The single place for the visual language: colours (RGB888 as uint32_t –
// LovyanGFX treats plain int literals as RGB565!), spacing and type scale.
// The web pages (web/*.html) mirror these values in their CSS variables.
#pragma once

#include <stdint.h>

namespace ui {
namespace theme {

// Surfaces
constexpr uint32_t kBg = 0x0D1015;         // app background, near-black blue
constexpr uint32_t kSurface = 0x171C24;    // cards, banner
constexpr uint32_t kSurfaceHi = 0x1E2530;  // highlighted row
constexpr uint32_t kDivider = 0x232A35;

// Text
constexpr uint32_t kText = 0xF3F5F8;
constexpr uint32_t kText2 = 0xA3ACBA;  // secondary
constexpr uint32_t kText3 = 0x677181;  // tertiary / hints
constexpr uint32_t kMissed = 0x56606E;  // departures you can no longer reach

// Semantics
constexpr uint32_t kAccent = 0x3DDC97;  // real-time, "leave in", primary actions
constexpr uint32_t kAccentInk = 0x06281A;
constexpr uint32_t kWarn = 0xF7B548;    // delays, stale data, notices
constexpr uint32_t kWarnInk = 0x2B1D05;
constexpr uint32_t kDanger = 0xFF6B6B;  // cancelled
constexpr uint32_t kInfo = 0x62B0FF;    // rain

// Weather icon palette
constexpr uint32_t kSun = 0xFFC94A;
constexpr uint32_t kMoon = 0xE9E4CF;
constexpr uint32_t kCloud = 0xDCE2EA;
constexpr uint32_t kCloudDark = 0x8C96A5;
constexpr uint32_t kRain = 0x62B0FF;
constexpr uint32_t kSnow = 0xF0F6FF;
constexpr uint32_t kBolt = 0xFFD84D;

// Geometry
constexpr int kW = 320;
constexpr int kH = 240;
constexpr int kBandH = 40;   // render band height (320x40x16bit = 25.6 kB)
constexpr int kPad = 10;     // outer horizontal padding
constexpr int kHeaderH = 58; // weather + clock header
constexpr int kBannerH = 24;
constexpr int kRowH = 34;
constexpr int kBadgeW = 40;
constexpr int kBadgeH = 22;
constexpr int kRadius = 8;

}  // namespace theme
}  // namespace ui
