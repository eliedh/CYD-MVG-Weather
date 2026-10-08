#include "band_renderer.h"

#include "fonts.h"

namespace ui {

bool BandRenderer::begin(lgfx::LovyanGFX* target) {
  target_ = target;
  fonts::init();
  sprite_.setColorDepth(16);
  sprite_.setPsram(false);
  if (!sprite_.createSprite(theme::kW, theme::kBandH)) return false;
  invalidate();
  return true;
}

void BandRenderer::invalidate() {
  for (int i = 0; i < kBands; i++) valid_[i] = false;
}

uint32_t BandRenderer::hashBuffer() {
  // FNV-1a over 32-bit words: ~6400 iterations per band, well under 1 ms.
  const uint32_t* p = (const uint32_t*)sprite_.getBuffer();
  size_t n = (size_t)theme::kW * theme::kBandH * 2 / 4;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

}  // namespace ui
