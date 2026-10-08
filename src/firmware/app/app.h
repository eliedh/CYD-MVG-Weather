// UI side of the firmware (runs in the Arduino loop on core 1): screen state
// machine, touch + BOOT button handling, backlight/night mode, rendering.
#pragma once

namespace app {

void setup();
void loop();

}  // namespace app
