#include "fonts.h"

#ifndef PROGMEM
#define PROGMEM
#endif

#include "fonts/font_badge.h"
#include "fonts/font_big.h"
#include "fonts/font_body.h"
#include "fonts/font_small.h"
#include "fonts/font_title.h"

namespace ui {
namespace fonts {

namespace {
struct Embedded {
  lgfx::PointerWrapper data;
  lgfx::VLWfont font;
  bool loaded = false;
  void load(const uint8_t* p, uint32_t len) {
    if (loaded) return;
    data.set(p, len);
    loaded = font.loadFont(&data);
  }
};
Embedded g_small, g_body, g_title, g_badge, g_big;
}  // namespace

void init() {
  g_small.load(font_small_vlw, font_small_vlw_len);
  g_body.load(font_body_vlw, font_body_vlw_len);
  g_title.load(font_title_vlw, font_title_vlw_len);
  g_badge.load(font_badge_vlw, font_badge_vlw_len);
  g_big.load(font_big_vlw, font_big_vlw_len);
}

const lgfx::IFont* small() { return &g_small.font; }
const lgfx::IFont* body() { return &g_body.font; }
const lgfx::IFont* title() { return &g_title.font; }
const lgfx::IFont* badge() { return &g_badge.font; }
const lgfx::IFont* big() { return &g_big.font; }

}  // namespace fonts
}  // namespace ui
