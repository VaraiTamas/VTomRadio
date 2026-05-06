#pragma once

#include "../../core/options.h"
#include <LovyanGFX.hpp>
#if DSP_MODEL != DSP_DUMMY
#    include "widgetsconfig.h"
#include "../display_select.h"
#include "textWidget.h"
#    define CHARWIDTH  6
#    define CHARHEIGHT 8

/*----------------------------------------------- BITRATE WIDGET -----------------------------------------------*/
class BitrateWidget : public Widget {
  public:
    BitrateWidget() {}

    BitrateWidget(BitrateBoxConfig boxconf, uint16_t fgcolor, uint16_t bgcolor) { init(boxconf, fgcolor, bgcolor); }
    ~BitrateWidget();
    using Widget::init;

    void init(BitrateBoxConfig boxconf, uint16_t fgcolor, uint16_t bgcolor);
    void setBitrate(uint16_t bitrate);
    void setFormat(BitrateFormat format);
    void clearAll();

  protected:
    LGFX_Sprite* _spr = nullptr;

    BitrateBoxConfig _box;
    BitrateFormat    _format;
    uint16_t         _bitrate;
    uint16_t         _dimension; // square esetén a négyzet oldalhossza, flat esetén a téglalap rövidebbik oldalának hossza

    char _buf[8];

    bool _usingVlw = false;

    void _ensureSprite();
    bool _applyFont();
    bool _isFlat();
    void refresh();

    void _draw();
    void _clear();
};

#endif