// Vector icons drawn with anti-aliased LovyanGFX primitives (no bitmaps), so
// they stay crisp at any size.
#pragma once

#include "model.h"
#include "painter.h"

namespace ui {
namespace icons {

// Weather icon centred at (cx, cy) fitting a size x size box. `bg` is the
// colour behind the icon (used for cut-outs such as the moon crescent).
void weather(Painter& p, core::WeatherIcon icon, bool isDay, int cx, int cy, int size,
             uint32_t bg);

void drop(Painter& p, int cx, int cy, int r, uint32_t color);
void clock(Painter& p, int cx, int cy, int r, uint32_t color, uint32_t bg);
void warning(Painter& p, int x, int y, int size, uint32_t color, uint32_t ink);
void wifi(Painter& p, int cx, int cy, int size, uint32_t color, bool crossed, uint32_t bg);
void chevronRight(Painter& p, int cx, int cy, int size, uint32_t color);
void liveDot(Painter& p, int cx, int cy, uint32_t color);
void logo(Painter& p, int x, int y, int size);

}  // namespace icons
}  // namespace ui
