#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    include "../display_select.h"
#    include "textWidget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

class ScrollWidget : public TextWidget {
  public:
    ScrollWidget() {}
    ScrollWidget(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor);
    ~ScrollWidget();
    using Widget::init;
    void init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor);
    void loop();
    void setText(const char* txt);
    void setText(const char* txt, const char* format);
    void setActive(bool act, bool clr);
    void setFontBySize(uint8_t size);
    void setColors(uint16_t fg, uint16_t bg) override;
    uint16_t textHeight() const { return _textheight; }

  private:
    enum ScrollState { SCROLL_WAIT_START, SCROLL_RUN };
    char*        _sep = nullptr;
    bool         _doscroll = false;
    uint8_t      _scrolldelta;
    uint16_t     _scrolltime;
    uint32_t     _scrolldelay;
    uint16_t     _startscrolldelay;
    LGFX_Sprite* _spr = nullptr;
    int          _scrollOffset = 0;
    int          _fullWidth = 0;
    ScrollState  _scrollState = SCROLL_WAIT_START;

    void _setTextParams();
    void _draw();
    void _clear();
    void _reset();
};
#endif