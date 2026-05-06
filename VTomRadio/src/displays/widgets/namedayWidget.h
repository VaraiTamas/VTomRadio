#pragma once

#include "widget.h"

#ifdef NAMEDAYS_FILE

class NamedayWidget : public Widget {
  public:
    ~NamedayWidget();

    using Widget::init;
  
    void init(NamedayWidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor);
    void draw(bool force = false);
    void clearNameday();

  protected:
    void _draw() override;
    void _clear() override;

  private:
    void _printNameday(bool force = false);
    void _getNamedayUpper(char* dest, size_t len);

  private:
    LGFX_Sprite* _spr = nullptr;

    uint16_t _spr_w = 0;
    uint16_t _spr_h = 0;

    void _ensureSprite();
    bool _applyFont(uint8_t size);

  private:
    NamedayWidgetConfig _namedayConf;

    char     _namedayBuf[32] = {0};
    char     _oldNamedayBuf[32] = {0};
    uint16_t _namedaywidth = 0;
    uint16_t _oldnamedaywidth = 0;
    uint32_t _lastRotation = 0;
};

#endif // NAMEDAYS_FILE