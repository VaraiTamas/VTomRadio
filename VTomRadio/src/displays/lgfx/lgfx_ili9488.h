#pragma once
#include <LovyanGFX.hpp>
#include "../../core/options.h" // Itt van definiálva a TS_MODEL és a TS_CS
#if TS_MODEL == TS_MODEL_AXS15231B
#include "touch_axs15231b.h"
#endif

class LGFX_ILI9488 : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9488 _panel;
    lgfx::Bus_SPI       _bus;

    // Touch példány deklarálása a modell alapján
#if TS_MODEL == TS_MODEL_XPT2046
    lgfx::Touch_XPT2046 _touch;
#elif TS_MODEL == TS_MODEL_GT911
    lgfx::Touch_GT911 _touch;
#elif TS_MODEL == TS_MODEL_FT6X36
    lgfx::Touch_FT6236 _touch; // Az LGFX ezt használja FT6X36-hoz
#elif TS_MODEL == TS_MODEL_AXS15231B
    lgfx::Touch_AXS15231B _touch;
#endif

  public:
    LGFX_ILI9488() {
        { // SPI busz beállítása
            auto cfg = _bus.config();
            cfg.spi_host = SPI3_HOST;
            cfg.pin_sclk = 12;
            cfg.pin_mosi = 11;
            cfg.pin_miso = 13; 
            cfg.pin_dc = 9;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000; 
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        { // Panel beállítása
            auto cfg = _panel.config();
            cfg.pin_cs = 10;
            cfg.pin_rst = -1;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
                        // This panel variant needs hardware inversion enabled for normal colors.
                        cfg.invert = true;
                        _panel.config(cfg);
        }

#if TS_MODEL != TS_MODEL_UNDEFINED
        { // Touch beállítása
            auto cfg = _touch.config();

#    if TS_MODEL == TS_MODEL_XPT2046
            cfg.pin_miso = 13;
            cfg.pin_cs = TS_CS;       // A touch chip select lába
            cfg.bus_shared = true;    // Közös busz az LCD-vel
            cfg.spi_host = SPI3_HOST; // Ugyanaz az SPI busz
            cfg.freq = 2500000;       // Az XPT2046 maximuma kb 2.5MHz
            cfg.offset_rotation = 0;  // Itt tudod korrigálni, ha a touch el van forgatva az LCD-hez képest (0-7 érték)
            cfg.bus_shared = true;    // Közös busz az LCD-vel
#    elif TS_MODEL == TS_MODEL_GT911 || TS_MODEL == TS_MODEL_FT6X36
            cfg.pin_sda = TS_SDA; // I2C pinek
            cfg.pin_scl = TS_SCL;
            cfg.i2c_port = 1;    // 0 vagy 1
            cfg.i2c_addr = 0x5D; // GT911 esetén (vagy 0x14)
            cfg.pin_int = -1;    // Ha van interrupt láb, ide írd a számát
            cfg.pin_rst = -1;    // Ha van reset láb, ide írd
#    elif TS_MODEL == TS_MODEL_AXS15231B
            cfg.pin_sda = TS_SDA;
            cfg.pin_scl = TS_SCL;
            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x3B;
            cfg.pin_int = TS_INT;
            cfg.pin_rst = TS_RST;
            cfg.x_min = 0;
            cfg.x_max = 319;
            cfg.y_min = 0;
            cfg.y_max = 479;
            cfg.offset_rotation = 0;
            cfg.bus_shared = false;
#    endif

            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
#endif
        setPanel(&_panel);
    }
};