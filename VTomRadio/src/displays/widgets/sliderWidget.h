#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
//#include "../display_select.h"
#include "widget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

/*-------------------------------------- SliderWidget ------------------------------------------*/
class SliderWidget : public Widget {
  public:
    SliderWidget() {}
    SliderWidget(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor = 0) { init(conf, fgcolor, bgcolor, maxval, oucolor); }
    using Widget::init;
    void init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor = 0);
    void setValue(uint32_t val);
    void setColors(uint16_t fg, uint16_t bg) override;

  protected:
    uint16_t _height, _oucolor, _oldvalwidth;
    uint32_t _max, _value;
    bool     _outlined;
    void     _draw();
    void     _drawslider();
    void     _clear();
    void     _reset();
};

#endif
