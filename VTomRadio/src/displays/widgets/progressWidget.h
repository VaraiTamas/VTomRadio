#pragma once

//#include "../../core/options.h"
#include "textWidget.h"
//#include <LovyanGFX.hpp>

#if DSP_MODEL != DSP_DUMMY
//#    include "widgetsconfig.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

class ProgressWidget : public TextWidget {
  public:
    ProgressWidget() {}
    ProgressWidget(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor) { init(conf, pconf, fgcolor, bgcolor); }
    using Widget::init;
    void init(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor) {
        TextWidget::init(conf, pconf.width, false, fgcolor, bgcolor);
        _speed = pconf.speed;
        _width = pconf.width;
        _barwidth = pconf.barwidth;
        _pg = 0;
    }
    void loop();

  private:
    uint8_t  _pg;
    uint16_t _speed, _barwidth;
    uint32_t _scrolldelay;
    void     _progress();
    bool     _checkDelay(int m, uint32_t& tstamp);
};
#endif