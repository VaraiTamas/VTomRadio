#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

class Widget {
  public:
    Widget() { _active = false; }
    virtual ~Widget() {}
    virtual void loop() {}
    virtual void init(WidgetConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
        _config = conf;
        _fgcolor = fgcolor;
        _bgcolor = bgcolor;
        _width = _backMove.width = 0;
        _backMove.x = _config.left;
        _backMove.y = _config.top;
        _moved = _locked = false;
    }
    void setAlign(WidgetAlign align) { _config.align = align; }
    virtual void setColors(uint16_t fg, uint16_t bg) { _fgcolor = fg; _bgcolor = bg; }

    void setActive(bool act, bool clr = false) {
        _active = act;
        if (_active && !_locked) { _draw(); }
        if (clr && !_locked) { _clear(); }
    }
        
    void lock(bool lck = true) {
        _locked = lck;
        if (_locked) { _reset(); }
        if (_locked && _active) { _clear(); }
        if (!_locked && _active) {
            _reset();
            _draw();
        }
    }
    void unlock() { _locked = false; }
    bool locked() { return _locked; }
    /* Widget átmozgatása.*/
    void moveTo(MoveConfig mv) {
        if (mv.width < 0) { return; }
        _moved = true;
        if (_active && !_locked) { _clear(); }
        _config.left = mv.x;
        _config.top = mv.y;
        if (mv.width > 0) { _width = mv.width; }
        _reset();
        _draw();
    }
    void moveBack() {
        if (!_moved) { return; }
        if (_active && !_locked) { _clear(); }
        _config.left = _backMove.x;
        _config.top = _backMove.y;
        _width = _backMove.width;
        _moved = false;
        _reset();
        _draw();
    }

  protected:
    bool         _active, _moved, _locked;
    uint16_t     _fgcolor, _bgcolor, _width;
    WidgetConfig _config;
    MoveConfig   _backMove;
    virtual void _draw() {}
    virtual void _clear() {}
    virtual void _reset() {}
};
#endif