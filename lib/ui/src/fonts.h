// Embedded smooth (anti-aliased) VLW fonts generated from Inter by
// tools/gen_fonts.py. The VLWfont objects and their data wrappers are created
// once and live forever – never call loadFont() per draw.
#pragma once

#include <LovyanGFX.hpp>

namespace ui {
namespace fonts {

void init();  // idempotent

const lgfx::IFont* small();  // Inter Medium 13 px, Latin-1
const lgfx::IFont* body();   // Inter SemiBold 17 px, Latin-1
const lgfx::IFont* title();  // Inter SemiBold 23 px, Latin-1
const lgfx::IFont* badge();  // Inter Bold 14 px, ASCII (line labels)
const lgfx::IFont* big();    // Inter Medium 44 px, digits/°/:/% only

}  // namespace fonts
}  // namespace ui
