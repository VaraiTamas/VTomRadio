#pragma once

#include <LovyanGFX.hpp>

#include "../../core/options.h"

#if TS_MODEL == TS_MODEL_AXS15231B
#include "touch_axs15231b.h"
#endif

template <typename TPanel>
class LGFX_Base : public lgfx::LGFX_Device {
  TPanel _panel;
  lgfx::Bus_SPI _bus;

#if TS_MODEL == TS_MODEL_XPT2046
  lgfx::Touch_XPT2046 _touch;
#elif TS_MODEL == TS_MODEL_GT911
  lgfx::Touch_GT911 _touch;
#elif TS_MODEL == TS_MODEL_FT6X36
  lgfx::Touch_FT5x06 _touch;
#elif TS_MODEL == TS_MODEL_AXS15231B
  lgfx::Touch_AXS15231B _touch;
#endif

  const int16_t _panelWidth;
  const int16_t _panelHeight;
  const uint8_t _rotationOffset;

  template <typename TCfg>
  void applySpiHost(TCfg& cfg) {
    cfg.spi_host = static_cast<decltype(cfg.spi_host)>(LGFX_LCD_SPI_HOST);
  }

  void configureBus() {
    auto cfg = _bus.config();
    applySpiHost(cfg);
    cfg.pin_sclk = 12;
    cfg.pin_mosi = 11;
    cfg.pin_miso = 13;
    cfg.pin_dc = TFT_DC;
    cfg.freq_write = 40000000;
    cfg.freq_read = 16000000;
    _bus.config(cfg);
    _panel.setBus(&_bus);
  }

  void configurePanel() {
    auto cfg = _panel.config();
    cfg.pin_cs = TFT_CS;
    cfg.pin_rst = TFT_RST;
    cfg.panel_width = _panelWidth;
    cfg.panel_height = _panelHeight;
    cfg.offset_rotation = _rotationOffset;
    cfg.invert = true;
    _panel.config(cfg);
  }

#if TS_MODEL != TS_MODEL_UNDEFINED
  void configureTouch() {
    auto cfg = _touch.config();

#if TS_MODEL == TS_MODEL_XPT2046
    cfg.pin_miso = 13;
    cfg.pin_cs = TS_CS;
    cfg.bus_shared = true;
    applySpiHost(cfg);
    cfg.freq = 2500000;
    cfg.offset_rotation = _rotationOffset;
#elif TS_MODEL == TS_MODEL_GT911 || TS_MODEL == TS_MODEL_FT6X36
    cfg.pin_sda = TS_SDA;
    cfg.pin_scl = TS_SCL;
    cfg.i2c_port = TS_I2C_PORT;
    cfg.i2c_addr = TS_I2C_ADDR;
    cfg.pin_int = (TS_INT == 255) ? -1 : TS_INT;
    cfg.pin_rst = (TS_RST == 255) ? -1 : TS_RST;
    cfg.x_min = 0;
    cfg.x_max = _panelWidth - 1;
    cfg.y_min = 0;
    cfg.y_max = _panelHeight - 1;
    cfg.offset_rotation = _rotationOffset;
    cfg.bus_shared = true;  // Wire is initialized in config.cpp on the same I2C port
#elif TS_MODEL == TS_MODEL_AXS15231B
    cfg.pin_sda = TS_SDA;
    cfg.pin_scl = TS_SCL;
    cfg.i2c_port = 1;
    cfg.i2c_addr = 0x3B;
    cfg.pin_int = TS_INT;
    cfg.pin_rst = TS_RST;
    cfg.x_min = 0;
    cfg.x_max = _panelWidth - 1;
    cfg.y_min = 0;
    cfg.y_max = _panelHeight - 1;
    cfg.offset_rotation = _rotationOffset;
    cfg.bus_shared = false;
#endif

    _touch.config(cfg);
    _panel.setTouch(&_touch);
  }
#endif

  public:
  LGFX_Base(int16_t panelWidth, int16_t panelHeight, uint8_t rotationOffset)
      : _panelWidth(panelWidth), _panelHeight(panelHeight), _rotationOffset(rotationOffset) {
    configureBus();
    configurePanel();
#if TS_MODEL != TS_MODEL_UNDEFINED
    configureTouch();
#endif
    setPanel(&_panel);
  }
};