#include "progressWidget.h"


void ProgressWidget::_progress() {
    char buf[_width + 1];
    snprintf(buf, _width, "%*s%.*s%*s", _pg <= _barwidth ? 0 : _pg - _barwidth, "", _pg <= _barwidth ? _pg : 5, ".....", _width - _pg, "");
    _pg++;
    if (_pg >= _width + _barwidth) { _pg = 0; }
    setText(buf);
}

bool ProgressWidget::_checkDelay(int m, uint32_t& tstamp) {
    if (millis() - tstamp > m) {
        tstamp = millis();
        return true;
    } else {
        return false;
    }
}

void ProgressWidget::loop() {
    if (_checkDelay(_speed, _scrolldelay)) { _progress(); }
}