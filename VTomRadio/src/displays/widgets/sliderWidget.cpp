#include "../../core/options.h"
#include "../../core/config.h"
#include "../display_select.h"
#include "Arduino.h"
#include "sliderWidget.h"

/**************************************************************************************************************
                                              SLIDER WIDGET (hangerő csík)
 **************************************************************************************************************/
void SliderWidget::init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor) {
    Widget::init(conf.widget, fgcolor, bgcolor);
    _width = conf.width;
    _height = conf.height;
    _outlined = conf.outlined;
    _oucolor = oucolor, _max = maxval;
    _oldvalwidth = _value = 0;
}

void SliderWidget::setValue(uint32_t val) {
    _value = val;
    if (_active && !_locked) { _drawslider(); }
}

void SliderWidget::_drawslider() {
    uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
    if (_oldvalwidth == valwidth) { return; }
    dsp.fillRect(_config.left + _outlined + min(valwidth, _oldvalwidth), _config.top + _outlined, abs(_oldvalwidth - valwidth), _height - _outlined * 2, _oldvalwidth > valwidth ? _bgcolor : _fgcolor);
    _oldvalwidth = valwidth;
}

void SliderWidget::_draw() {
    if (_locked) { return; }
    _clear();
    if (!_active) { return; }
    if (_outlined) { dsp.drawRect(_config.left, _config.top, _width, _height, _oucolor); }
    uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
    dsp.fillRect(_config.left + _outlined, _config.top + _outlined, valwidth, _height - _outlined * 2, _fgcolor);
}

void SliderWidget::_clear() {
    dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}
void SliderWidget::_reset() {
    _oldvalwidth = 0;
}

void SliderWidget::setColors(uint16_t fg, uint16_t bg) {
    _fgcolor = fg;
    _bgcolor = bg;
    _oldvalwidth = 0;  // force full redraw
    _draw();
}