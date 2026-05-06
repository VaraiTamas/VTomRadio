#pragma once

#include "../../core/options.h"
#include "widget.h"
#include <LovyanGFX.hpp>

#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

class VuWidget : public Widget {
  public:
    VuWidget() {}
    VuWidget(VU_WidgetConfig wconf,
             uint16_t vumaxcolor, uint16_t vumidcolor,
             uint16_t vumincolor, uint16_t bgcolor) {
        init(wconf, vumaxcolor, vumidcolor, vumincolor, bgcolor);
    }
    VuWidget(VU_WidgetConfig wconf,
             VU_WidgetConfig wconfBidirectional,
             uint16_t vumaxcolor, uint16_t vumidcolor,
             uint16_t vumincolor, uint16_t bgcolor) {
        init(wconf, wconfBidirectional, vumaxcolor, vumidcolor, vumincolor, bgcolor);
    }

    ~VuWidget();

    using Widget::init;

    void        init(VU_WidgetConfig wconf,
                     uint16_t vumaxcolor, uint16_t vumidcolor,
                     uint16_t vumincolor, uint16_t bgcolor);
    void        init(VU_WidgetConfig wconf,
             VU_WidgetConfig wconfBidirectional,
             uint16_t vumaxcolor, uint16_t vumidcolor,
             uint16_t vumincolor, uint16_t bgcolor);
    void        switchMode(bool bidirectional);
    void        loop();

    static void setLabelsDrawn(bool value);
    static bool isLabelsDrawn();

  protected:
#    if defined(DSP_OLED)
    uint16_t _maxDimension = 216;
    uint16_t _peakL = 0;
    uint16_t _peakR = 0;
    uint8_t  _peakFallDelay = 6;
    uint8_t  _peakFallRate = 1;
    uint8_t  _peakFallDelayCounter = 0;
#    endif

#    if !defined(DSP_OLED)
    LGFX_Sprite* _spr = nullptr;
#    endif

    static bool   _labelsDrawn;
    VU_WidgetConfig _vuConfNormal;
    VU_WidgetConfig _vuConfBidirectional;
    VU_WidgetConfig _vuConf;
    uint16_t      _vumaxcolor;
    uint16_t      _vumidcolor;
    uint16_t      _vumincolor;
    bool          _bidirectional = false;

    void _draw();
    void _clear();
  #    if !defined(DSP_OLED)
    void _recreateSprite();
  #    endif
};

#endif