#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
// #include "../display_select.h"
#    include "textWidget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

class NumWidget : public TextWidget {
  public:
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor);
    ~NumWidget();
    void setText(const char* txt);
    void setText(int val, const char* format);
    void setColors(uint16_t fg, uint16_t bg) override;

  protected:
    LGFX_Sprite* _spr = nullptr;
    void         _getBounds();
    void         _draw();
};

#endif