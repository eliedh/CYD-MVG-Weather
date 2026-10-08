#pragma once

namespace net {

// Starts the network task pinned to core 0 (16 KB stack). It owns Wi-Fi,
// the captive-portal DNS, NTP, mDNS, the web server and all HTTPS fetching.
void start();

}  // namespace net
