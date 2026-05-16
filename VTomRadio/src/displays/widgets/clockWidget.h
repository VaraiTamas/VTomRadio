#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    include "../display_select.h"
#    include "textWidget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

/*----------------------------------------------- CLOCK WIDGET -----------------------------------------------*/
class ClockWidget : public Widget {
  public:
    using Widget::init;
    void            init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor);
    void            draw(bool force = false);
    uint8_t         textsize() { return _config.textsize; }
    void            clear() { _clearClock(); if (_spr && _spr->getBuffer()) _spr->pushSprite(_clockleft, _config.top); }
    inline uint16_t dateSize() { return _space + _dateheight; }
    inline uint16_t clockWidth() { return _clockwidth; }
    uint16_t        height() const { return _spr ? _spr->height() : 0; }

  private:
#    if defined(DSP_OLED) && (DSP_MODEL == DSP_SSD1322)
    void _drawShortDateSSD1322();
#    endif

  protected:
    char     _timebuffer[20] = "00:00";
    char     _tmp[38], _datebuf[38];
    uint16_t _clockleft, _clockwidth, _timewidth, _dotsleft, _dotswidth, _linesleft;
    uint8_t  _clockheight, _timeheight, _dateheight, _space;
    uint16_t _forceflag = 0;
    uint16_t _secHeight, _secTopSpace;
    bool     dots = true;
    // bool              _fullclock;
    LGFX_Sprite* _spr = nullptr;
    LGFX_Sprite* _dateSpr = nullptr;
    WidgetConfig _clockConf;
    WidgetConfig _dateConf;
    const void*  _lastMainFont = nullptr;
    const void*  _lastSecFont = nullptr;
    uint8_t      _lastClockFontStyle = 0xFF;
    bool         _lastClockAmPmStyle = false;
    int16_t      _lastRenderedHour = -1;
    int16_t      _lastRenderedMinute = -1;
    int16_t      _lastRenderedSecond = -1;
    bool         _lastRenderedDots = false;
    tm           _drawTimeinfo{};
    void         _draw();
    void         _clear();
    void         _reset();
    void         _getTimeBounds();
    void         _printClock(bool force = false);
    void         _clearClock();
    void         _formatDate();
    bool         _getTime();
    void         _captureTimeSnapshot();
    void         _calcSize();
    bool         _syncLayoutIfNeeded(bool forceRedraw);
    uint16_t     _left();
    uint16_t     _top();
    void         _begin();
};

#endif