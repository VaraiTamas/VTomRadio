#    include "../../core/config.h"
#    include "../display_select.h"
#    include "Arduino.h"
#include "fillWidget.h"

void FillWidget::init(FillConfig conf, uint16_t bgcolor) {
    Widget::init(conf.widget, bgcolor, bgcolor);
    _width = conf.width;
    _height = conf.height;
}

void FillWidget::_draw() {
    if (!_active) { return; }
    dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}

void FillWidget::setHeight(uint16_t newHeight) {
    _height = newHeight;
}

void FillWidget::setColors(uint16_t fg, uint16_t bg) {
    _fgcolor = bg;  // FillWidget uses bgcolor for fill
    _bgcolor = bg;
    _draw();
}