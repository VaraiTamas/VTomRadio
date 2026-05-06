#include "textWidget.h"
#include "../../core/options.h"
#    include "../../core/config.h"
#    include "../display_select.h"
#    include "Arduino.h"
#    include "widgets.h"
#    include <lgfx/v1/lgfx_fonts.hpp>
#    include "../../core/fonts.h"


TextWidget::~TextWidget() {
    free(_text);
    free(_oldtext);
}

void TextWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height) {
    width = textsize * CHARWIDTH;
    height = textsize * CHARHEIGHT;
}

void TextWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
    Widget::init(wconf, fgcolor, bgcolor);
    _buffsize = buffsize;
    _text = (char*)malloc(sizeof(char) * _buffsize);
    memset(_text, 0, _buffsize);
    _oldtext = (char*)malloc(sizeof(char) * _buffsize);
    memset(_oldtext, 0, _buffsize);
    _charSize(_config.textsize, _charWidth, _textheight);
    _textwidth = _oldtextwidth = _oldleft = 0;
    _uppercase = uppercase;
}

void TextWidget::setText(const char* txt) {
    strlcpy(_text, txt, _buffsize);
    _textwidth = strlen(_text) * _charWidth;
    if (strcmp(_oldtext, _text) == 0) { return; }

    if (_active) { dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()), _config.top, max(_oldtextwidth, _textwidth), _textheight, _bgcolor); }

    _oldtextwidth = _textwidth;
    _oldleft = _realLeft();
    if (_active) { _draw(); }
}

void TextWidget::setText(int val, const char* format) {
    char buf[_buffsize];
    snprintf(buf, _buffsize, format, val);
    setText(buf);
}

void TextWidget::setText(const char* txt, const char* format) {
    char buf[_buffsize];
    snprintf(buf, _buffsize, format, txt);
    setText(buf);
}

uint16_t TextWidget::_realLeft(bool w_fb) {
    uint16_t realwidth = (_width > 0 && w_fb) ? _width : dsp.width();
    switch (_config.align) {
        case WA_CENTER: return (realwidth - _textwidth) / 2; break;
        case WA_RIGHT: return (realwidth - _textwidth - (!w_fb ? _config.left : 0)); break;
        default: return !w_fb ? _config.left : 0; break;
    }
}

void TextWidget::_draw() {
    if (!_active) { return; }
    dsp.setTextColor(_fgcolor, _bgcolor);
    dsp.setCursor(_realLeft(), _config.top);
    dsp.setFont(nullptr);
    dsp.setTextSize(_config.textsize);
    dsp.print(_text);
    strlcpy(_oldtext, _text, _buffsize);
}
