// LovyanGFX device for the CYD. Panel driver and inversion come from
// board_config.h (compile-time switches, no auto-detection).
#pragma once

#include <LovyanGFX.hpp>

#include "board_config.h"

class LGFX_CYD : public lgfx::LGFX_Device {
#if CYD_PANEL_ST7789
  lgfx::Panel_ST7789 panel_;
#else
  lgfx::Panel_ILI9341 panel_;
#endif
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

 public:
  LGFX_CYD() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;  // HSPI
      cfg.spi_mode = 0;
      cfg.freq_write = TFT_SPI_WRITE_HZ;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_TFT_SCLK;
      cfg.pin_mosi = PIN_TFT_MOSI;
      cfg.pin_miso = PIN_TFT_MISO;
      cfg.pin_dc = PIN_TFT_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = PIN_TFT_CS;
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = PANEL_INVERT;
      cfg.rgb_order = PANEL_BGR;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = PIN_TFT_BL;
      cfg.invert = false;
      cfg.freq = TFT_BL_PWM_HZ;
      cfg.pwm_channel = TFT_BL_PWM_CHANNEL;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    {
      auto cfg = touch_.config();
      cfg.x_min = TOUCH_X_MIN;
      cfg.x_max = TOUCH_X_MAX;
      cfg.y_min = TOUCH_Y_MIN;
      cfg.y_max = TOUCH_Y_MAX;
      cfg.pin_int = PIN_TOUCH_IRQ;
      cfg.bus_shared = false;
      cfg.offset_rotation = TOUCH_OFFSET_ROTATION;
      cfg.spi_host = SPI3_HOST;  // VSPI, separate from the display
      cfg.freq = 1000000;
      cfg.pin_sclk = PIN_TOUCH_SCLK;
      cfg.pin_mosi = PIN_TOUCH_MOSI;
      cfg.pin_miso = PIN_TOUCH_MISO;
      cfg.pin_cs = PIN_TOUCH_CS;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};
