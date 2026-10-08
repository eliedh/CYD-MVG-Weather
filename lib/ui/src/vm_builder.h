// Turns settings + data snapshot + time into the main-screen view model.
// Shared by the firmware and the host preview.
#pragma once

#include "settings.h"
#include "snapshot.h"
#include "view_model.h"

namespace ui {

void fillFromSnapshot(ViewModel& vm, const core::Settings& s, const core::DataSnapshot& d,
                      int64_t now, bool timeValid);

}  // namespace ui
