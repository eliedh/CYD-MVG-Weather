# Hardware test status (owner's two-USB ST7789 board)

| # | Test | Status |
|---|---|---|
| 1 | Flash, boot, display orientation/colours | ✅ works |
| 2 | Setup hotspot + captive portal + Wi-Fi join | ✅ works |
| 3 | Stop search (first HTTPS, was OOM crash) | ✅ fixed, works |
| 4 | Departures + weather from the real APIs | ✅ works |
| 5 | Buses missing at "Gautinger Straße" (Stockdorf) | ✅ fixed (explicit `transportTypes`), regional buses show |
| 6 | Service messages: stream stopped after 7616 bytes (1 s Stream timeout) | ✅ fixed – full 389 kB feed read in ~4 s on the device |
| 6b | Departure list paging: tap list → pages 2–4, dots, back to page 1 after 20 s | ⏳ to verify |
| 7 | LDR: `LDR raw=` values in the dark (bright reads 0) | ⏳ to do |
| 8 | Touch: header → forecast, long-press → settings QR, reset Cancel, calibration | ⏳ to do |
| 9 | Night mode dim/dark + tap-to-wake | ⏳ to do |
| 10 | Power cycle keeps settings; Wi-Fi loss → "WLAN getrennt", stale marker, recovery | ⏳ to do |
| 11 | BOOT without serial monitor (GPIO0 reads LOW while monitor attached) | ⏳ to do |
| 12 | Overnight run: no restart ("Letzter Neustart" = Eingesteckt), heap `min` > 40 kB | ⏳ to do |
| 13 | Full reset on device → hotspot → setup from scratch (iOS + Android) | ⏳ to do |

Healthy numbers from the first good log: ~107 kB free heap at rest, min 52.8 kB
during TLS, net task stack ~9.9 kB left.
