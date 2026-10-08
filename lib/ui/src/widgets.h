#pragma once

#include <string>

#include "line_style.h"
#include "painter.h"

namespace ui {
namespace widgets {

// Line badge in MVG/MVV colour & shape. Width grows for long labels.
// Returns the badge width actually used.
int lineBadge(Painter& p, int x, int y, int w, int h, core::TransportType type,
              const char* label, bool dimmed = false);

// QR code on a white rounded card (quiet zone included). `size` is the card size.
// The module matrix is cached per text, so repeated band draws are cheap.
void qrCard(Painter& p, const char* text, int x, int y, int size);

// Numbered step bullet (accent circle with number).
void stepBullet(Painter& p, int cx, int cy, int n);

// Three-dot activity indicator; `phase` cycles 0..2.
void dots(Painter& p, int cx, int cy, int phase, uint32_t color);

// Paints a multi-line string (already wrapped) and returns the next baseline.
int lines(Painter& p, const std::vector<std::string>& ls, int x, int baseline, int lineH,
          const lgfx::IFont* font, uint32_t color, int maxLines = 99);

}  // namespace widgets
}  // namespace ui
