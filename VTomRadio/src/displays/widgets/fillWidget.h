#pragma once
#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    include "widget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

/*------------------------------------------ FillWidget --------------------------------------------*/
class FillWidget : public Widget {
  public:
    FillWidget() {}
    FillWidget(FillConfig conf, uint16_t bgcolor) { init(conf, bgcolor); }
    using Widget::init;
    void init(FillConfig conf, uint16_t bgcolor);
    void setHeight(uint16_t newHeight);
    void setColors(uint16_t fg, uint16_t bg) override;

  protected:
    uint16_t _height;
    void     _draw();
};
#endif