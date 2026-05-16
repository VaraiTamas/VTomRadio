#pragma once
#include "common.h"

#if DSP_MODEL == DSP_DUMMY
#    define DUMMYDISPLAY
#endif

#ifndef DUMMYDISPLAY
class ScrollWidget;
class PlayListWidget;
class BitrateWidget;
class FillWidget;
class SliderWidget;
class Pager;
class Page;
class VuWidget;
class NumWidget;
class ClockWidget;
class TextWidget;
class textBoxWidget;
class VolumeWidget;
class WifiWidget;
class NamedayWidget;

class Display {
  public:
    uint16_t      currentPlItem;
    uint16_t      numOfNextStation;
    displayMode_e _mode;

  public:
    Display() {};
    ~Display();
    displayMode_e mode() { return _mode; }
    void          mode(displayMode_e m) { _mode = m; }
    void          init();
    void          loop();
    void          _start();
    bool          ready() { return _bootStep == 2; }
    void          resetQueue();
    void          purgeQueuedRequestType(displayRequestType_e type);
    void          putRequest(displayRequestType_e type, int payload = 0);
    void          flip();
    void          invert();
    bool          deepsleep();
    void          wakeup();
    void          setContrast();
    void          lock() { _locked = true; }
    void          unlock() { _locked = false; }
    uint16_t      width();
    uint16_t      height();
    void          setBrightnessPercent(uint8_t percent);
    void          applyVuModeChange();
    void          invalidateThemeWidgets();  // Refresh all widgets when theme colors change
#    ifdef NAMEDAYS_FILE
    void loopNameday(bool force = false);
#    endif

  private:
    ScrollWidget *  _meta, *_title1, *_plcurrent, *_weather, *_title2;
    PlayListWidget* _plwidget;
    BitrateWidget*  _bitratewidget;
    FillWidget *    _metabackground, *_plbackground;
    SliderWidget *  _volbar, *_heapbar;
    Pager*          _pager;
    Page*           _footer;
    VuWidget*       _vuwidget;
    NumWidget*      _nums;
    ClockWidget*    _clock;
    Page*           _boot;
    TextWidget *    _volip, *_voltxt, *_rssi, *_bitrate, *_chtxt;
    textBoxWidget * _ipbox, *_chbox, *_rssibox, *_bootstring;
    VolumeWidget*   _volwidget;
    WifiWidget*     _wifiwidget;
    NamedayWidget*  _namedaywidget;

    bool    _locked = false;
    uint8_t _bootStep;
    void    _time(bool redraw = false);
    void    _apScreen();
    void    _swichMode(displayMode_e newmode);
    void    _drawPlaylist();
    void    _volume();
    void    _title();
    void    _station();
    void    _drawNextStationNum(uint16_t num);
    void    _createDspTask();
    void    _showDialog(const char* title);
    void    _buildPager();
    void    _bootScreen();
    void    _layoutChange(bool played);
    void    _setRSSI(int rssi);
    void    _updateBootSprite(int ssidIndex);
    void    _refreshThemeColors();
    void    _applyRssiMode();
};

#else

class Display {
  public:
    uint16_t      currentPlItem;
    uint16_t      numOfNextStation;
    displayMode_e _mode;

  public:
    Display() {};
    displayMode_e mode() { return _mode; }
    void          mode(displayMode_e m) { _mode = m; }
    void          init();
    void          _start();
    void          putRequest(displayRequestType_e type, int payload = 0);
    void          loop() {}
    bool          ready() { return true; }
    void          resetQueue() {}
    void          centerText(const char* text, uint8_t y, uint16_t fg, uint16_t bg) {}
    void          rightText(const char* text, uint8_t y, uint16_t fg, uint16_t bg) {}
    void          flip() {}
    void          invert() {}
    void          setContrast() {}
    void          applyVuModeChange() {}
    bool          deepsleep() { return true; }
    void          wakeup() {}
    void          printPLitem(uint8_t pos, const char* item) {}
    void          lock() {}
    void          unlock() {}
    uint16_t      width() { return 0; }
    uint16_t      height() { return 0; }

  private:
    void _createDspTask();
};

#endif

void display_show_maintenance_screen();

extern Display display;
