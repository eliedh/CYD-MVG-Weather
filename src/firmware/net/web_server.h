// HTTP server on port 80: captive setup portal (hotspot mode) and the
// settings page + JSON API (home network). Pages are gzipped in flash.
#pragma once

namespace web {

void begin();
void setPortalMode(bool on);

}  // namespace web
