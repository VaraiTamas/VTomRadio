#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#include "widget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

class TextWidget : public Widget {
  public:
    TextWidget() {}
    TextWidget(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) { init(wconf, buffsize, uppercase, fgcolor, bgcolor); }
    ~TextWidget();
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor);
    void setText(const char* txt);
    void setText(int val, const char* format);
    void setText(const char* txt, const char* format);
    bool uppercase() { return _uppercase; }

  protected:
    char*    _text;
    char*    _oldtext;
    bool     _uppercase;
    uint16_t _buffsize, _textwidth, _oldtextwidth, _oldleft, _textheight;
    uint8_t  _charWidth;

  protected:
    void     _draw();
    uint16_t _realLeft(bool w_fb = false);
    void     _charSize(uint8_t textsize, uint8_t& width, uint16_t& height);
};

#endif