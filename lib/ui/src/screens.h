#pragma once

#include "painter.h"
#include "view_model.h"

namespace ui {

// Draws vm.screen. Called once per band; must only depend on `vm`.
void drawScreen(Painter& p, const ViewModel& vm);

// Main screen layout helpers (shared with touch handling).
bool mainHasBanner(const ViewModel& vm);
int mainVisibleRows(const ViewModel& vm);

enum class HitZone : uint8_t { None, Header, Banner, Body };
HitZone hitTestMain(const ViewModel& vm, int x, int y);

// Number of pages the current message needs (uses `measure` for text widths).
int messagePageCount(lgfx::LovyanGFX& measure, const ViewModel& vm);

}  // namespace ui
