// Line badge colours and shapes, following MVG/MVV conventions.
// Colours are RGB888. Values are close approximations of the official
// palette (see DECISIONS.md); adjust here if you have the exact brand values.
#pragma once

#include <stdint.h>

#include "model.h"

namespace core {

enum class BadgeShape : uint8_t {
  Rect,     // U-Bahn, tram: slightly rounded rectangle
  Pill,     // S-Bahn: fully rounded ends
  Rounded,  // buses: clearly rounded corners
};

struct LineStyle {
  uint32_t bg;
  uint32_t fg;
  uint32_t bg2;  // second colour for split badges (U7, U8); == bg otherwise
  BadgeShape shape;
};

LineStyle lineStyle(TransportType type, const char* label);

}  // namespace core
