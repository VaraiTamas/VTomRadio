#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#    include "../display_select.h"
#    include "widget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

#    define MAX_PL_PAGE_ITEMS 15

class PlayListWidget : public Widget {
  public:
    using Widget::init;
    PlayListWidget();
    virtual ~PlayListWidget();
    void     init(ScrollWidget* current);
    void     drawPlaylist(uint16_t currentItem);
    uint16_t itemHeight() { return _plItemHeight; }
    uint16_t currentTop() { return (dsp.height() / 2) - (_plItemHeight / 2); }

  private:
    ScrollWidget* _currentScroll = nullptr;
    uint16_t      _plTtemsCount = 0, _plCurrentPos = 0;
    int           _plYStart = 0;
    uint16_t      _plItemHeight = 30;
    uint8_t       _sideRows = 3;
    uint16_t      _lastIndex = 0xFFFF;
    bool          _lastMovingMode = false;
    int16_t       _plLoadedPage = -1;
    int16_t       _plLastGlobalPos = -1;
    uint32_t      _plLastDrawTime = 0;
    uint8_t       _lastScrollFontSize = 0;
    String        _nameCache[15];
    int           _cacheBaseIdx = -1;
    String        _plCache[MAX_PL_PAGE_ITEMS];
    void          _updateNameCache(int centerIdx);
    String        _getCachedName(int index);
    void          _renderStaticList(uint16_t centerIdx);
    String        _fetchStationName(int index);
    void          _loadPlaylistPage(int pageIndex, int itemsPerPage, int totalItems);
    void          _printPLitem(uint8_t pos, const char* item);
    void          _printPLitem(uint8_t pos, const char* item, bool usePreloadedFont);
    void          _loadDspFont();
    void          _prepareMovingModeLayout();
    void          _prepareFixedModeLayout();
};

#endif