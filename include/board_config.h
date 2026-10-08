// Board / panel configuration for the ESP32-2432S028R "Cheap Yellow Display".
//
// ┌──────────────────────────────┬──────────────────┬──────────────┐
// │ Board                        │ CYD_PANEL_ST7789 │ PANEL_INVERT │
// ├──────────────────────────────┼──────────────────┼──────────────┤
// │ Two-USB CYD (micro-USB+USB-C)│        1         │      0       │  ← default, verified
// │ Classic single micro-USB CYD │        0 (ILI9341)│     0       │
// └──────────────────────────────┴──────────────────┴──────────────┘
//
// Symptom guide (see README):
//   * scrambled / repeated / shifted image  -> wrong driver: flip CYD_PANEL_ST7789
//   * light background, greens look pink/red -> flip PANEL_INVERT
//   * red and blue swapped (U3 orange looks blue) -> flip PANEL_BGR
// Panel ID reads are unreliable on these boards, so there is NO auto-detection.
// The PlatformIO envs cyd_st7789 / cyd_ili9341 set these via build flags.
#pragma once

#ifndef CYD_PANEL_ST7789
#define CYD_PANEL_ST7789 1
#endif
#ifndef PANEL_INVERT
#define PANEL_INVERT 0
#endif
#ifndef PANEL_BGR
#define PANEL_BGR 0
#endif
// 1 = landscape with the USB ports on the right (verified on the two-USB board).
#ifndef PANEL_ROTATION
#define PANEL_ROTATION 1
#endif

// ---- Display: HSPI (SPI2_HOST) ---------------------------------------------
#define PIN_TFT_SCLK 14
#define PIN_TFT_MOSI 13
#define PIN_TFT_MISO 12
#define PIN_TFT_DC 2
#define PIN_TFT_CS 15
#define PIN_TFT_RST -1
#define PIN_TFT_BL 21
#define TFT_SPI_WRITE_HZ 40000000
#define TFT_BL_PWM_CHANNEL 7
#define TFT_BL_PWM_HZ 12000

// ---- Touch: XPT2046 on its own bus (SPI3_HOST) --------------------------------
#define PIN_TOUCH_SCLK 25
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CS 33
#define PIN_TOUCH_IRQ 36
// Raw range used until the user runs the calibration (stored in NVS afterwards).
#ifndef TOUCH_X_MIN
#define TOUCH_X_MIN 300
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3700
#endif
#ifndef TOUCH_OFFSET_ROTATION
#define TOUCH_OFFSET_ROTATION 0
#endif

// ---- Misc I/O -------------------------------------------------------------------
#define PIN_LED_R 4  // RGB LED, active LOW, very dim: switched off at boot
#define PIN_LED_G 16
#define PIN_LED_B 17
#define PIN_BOOT_BUTTON 0  // active LOW

// ---- Light sensor (LDR) -------------------------------------------------------------
// Reads ~0 in bright light and higher values in the dark. Raw readings are
// logged every 30 s ("LDR raw=...") - use them to calibrate these thresholds.
#define PIN_LDR 34
#ifndef LDR_BRIGHT_RAW
#define LDR_BRIGHT_RAW 40  // at or below: bright room -> full user brightness
#endif
#ifndef LDR_DARK_RAW
#define LDR_DARK_RAW 600  // at or above: dark room -> LDR_MIN_PERCENT of user brightness
#endif
#ifndef LDR_MIN_PERCENT
#define LDR_MIN_PERCENT 25
#endif

// ---- Product -------------------------------------------------------------------------
#define PRODUCT_NAME "Abfahrt"
#define PRODUCT_HOSTNAME "abfahrt"  // -> http://abfahrt.local
#define FIRMWARE_VERSION "1.0.0"
