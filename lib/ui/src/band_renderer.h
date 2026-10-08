// Renders a full screen through ONE 320x40 16-bit sprite (25.6 kB), band by
// band. A full framebuffer does not fit reliably without PSRAM.
//
// After drawing a band, its pixels are hashed; the band is only pushed to the
// display when the hash differs from what is already on screen. This gives
// "redraw only what changed" (e.g. a countdown minute) without bookkeeping in
// the screens, and keeps updates flicker-free.
//
// pushSprite() may use DMA and return before the transfer finished, so the
// renderer calls target->waitDMA() before drawing into the buffer again.
// Deliberately no double buffering (it garbled the display on this board).
#pragma once

#include <LovyanGFX.hpp>

#include "painter.h"
#include "theme.h"

namespace ui {

class BandRenderer {
 public:
  static constexpr int kBands = theme::kH / theme::kBandH;

  // `target` is the panel (device) or a full-size sprite (host preview).
  bool begin(lgfx::LovyanGFX* target);

  template <typename DrawFn>
  int render(DrawFn&& draw, bool force = false) {
    int pushed = 0;
    for (int b = 0; b < kBands; b++) {
      target_->waitDMA();  // buffer may still be read by the previous push
      Painter p(sprite_, b * theme::kBandH, theme::kBandH);
      draw(p);
      uint32_t h = hashBuffer();
      if (force || !valid_[b] || h != hash_[b]) {
        sprite_.pushSprite(target_, 0, b * theme::kBandH);
        hash_[b] = h;
        valid_[b] = true;
        pushed++;
      }
    }
    return pushed;
  }

  // Forces every band to be pushed on the next render (e.g. after something
  // drew directly on the panel, like the touch calibration).
  void invalidate();
  void finish() { target_->waitDMA(); }
  LGFX_Sprite& sprite() { return sprite_; }

 private:
  uint32_t hashBuffer();

  lgfx::LovyanGFX* target_ = nullptr;
  LGFX_Sprite sprite_;
  uint32_t hash_[kBands] = {0};
  bool valid_[kBands] = {false};
};

}  // namespace ui
