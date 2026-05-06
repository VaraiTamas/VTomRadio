/* WiFi image: https://pixabay.com/illustrations/wifi-transparent-wifi-png-wireless-1371033/*/

#pragma once

#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include <LovyanGFX.hpp>
#    include "widgetsconfig.h"
#    include "../display_select.h"
#    include "widget.h"

class WifiWidget : public Widget {
  public:
    // 🔥 EZ KELL NEKED
    WifiWidget(yoDisplay* dsp, const WifiWidgetConfig& cfg);
    WifiWidget(yoDisplay* dsp, const WifiWidgetConfig* cfg); // PROGMEM-hez

    ~WifiWidget();

    void setStrength(int8_t s);
    void setRSSI(int32_t rssi);

  protected:
    void _draw() override;
    void _clear() override;
    void _reset() override;

  private:
    yoDisplay*       _dsp;
    WifiWidgetConfig _cfg;
    LGFX_Sprite*     _spr = nullptr;
    uint8_t*         _imgBuf = nullptr;
    size_t           _imgSize = 0;
    int8_t           _imgLevel = -1;

    int8_t _strength = 0;
    bool   _dirty = true;

    void _init();
    void _ensureSprite();
    void _deleteSprite();
    void _freeImageBuffer();
    bool _loadImageForLevel(int8_t level);
    bool _drawImageForStrength();
    const char* _pathForLevel(int8_t level) const;

    void _drawWifi();
    void _fillArcThick(int32_t cx, int32_t cy, int32_t outerR, int32_t thickness, int32_t start, int32_t end, uint16_t color);

    uint16_t _getActiveColor() const;

    int8_t _clamp(int v);
    int8_t _rssiToStrength(int32_t rssi);
};
#endif