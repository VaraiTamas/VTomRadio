#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    include "widget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

struct textBoxConfig {
    uint16_t    left;
    uint16_t    top;
    uint16_t    width;
    uint16_t    height;
    uint8_t     textsize;
    WidgetAlign align;
    uint8_t     border;
    uint8_t     radius;
    bool        fill;
    uint16_t    paddingX;
    uint16_t    paddingY;
};

class textBoxWidget : public Widget {
  public:
    textBoxWidget() {}

    textBoxWidget(textBoxConfig conf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor, uint16_t borderColor) {

        _fgcolor = fgcolor;
        _bgcolor = bgcolor;
        _borderColor = borderColor;

        init(conf, buffsize, uppercase);
    }

    ~textBoxWidget();

    using Widget::init;

    void init(textBoxConfig conf, uint16_t buffsize, bool uppercase = false);

    void setText(const char* txt);
    void setText(int val, const char* format);
    void setText(const char* txt, const char* format);

    bool uppercase() { return _uppercase; }
    void setColors(uint16_t fg, uint16_t bg, uint16_t border) {
        _fgcolor = fg;
        _bgcolor = bg;
        _borderColor = border;
        if (_active) { _draw(); }
    }

  protected:
    textBoxConfig _boxconf;
    uint16_t      _fgcolor;
    uint16_t      _bgcolor;
    uint16_t      _borderColor;

    char*    _text = nullptr;
    char*    _oldtext = nullptr;
    bool     _uppercase = false;
    uint16_t _buffsize = 0;

    uint16_t _textwidth = 0;
    uint16_t _textheight = 0;

    LGFX_Sprite* _spr = nullptr;

    uint8_t _loadedFontSize = 0xFF;
    bool    _loadedAsVlw = false;

    // static constexpr uint16_t _transparentKey = 0xF81F; // magenta key

  protected:
    void _draw() override;
    void _clear() override;

    void _ensureSprite();
    bool _applyFont();
    void _resetFontCache();
};
#endif