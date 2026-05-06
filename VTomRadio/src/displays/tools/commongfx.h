#pragma once
#include "../display_select.h"
#include "../widgets/widgetsconfig.h" // displayXXXDDDDconf.h
#include <FS.h>
#include <LittleFS.h>
#define LGFX_USE_VFS

typedef struct clipArea {
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
} clipArea;

class DspCore : public yoDisplay { // yoDisplay: typedef LGFX yoDisplay

  public:
    LGFX_Sprite* getBootSprite();
    DspCore();
    void initDisplay();
    void createBootSprite();
    void deleteBootSprite();
    void clearDsp(bool black = false);
    void printClock() {}
    // 1. Verzió: Fájlnévhez (ezt már beírtuk)
    bool loadFont(const char* path) { return yoDisplay::loadFont(path); }
    // 2. Verzió: Memóriából való betöltéshez (EZ HIÁNYZIK!)
    bool loadFont(const uint8_t* data) { return yoDisplay::loadFont(data); }
    void unloadFont() { yoDisplay::unloadFont(); }

#ifdef DSP_OLED
    inline void loop(bool force = false) {
        display();
        vTaskDelay(DSP_MODEL == DSP_ST7920 ? 10 : 0);
    }
    inline void drawLogo(uint16_t top) {
        drawBitmap((width() - LOGO_WIDTH) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT, 1);
        this->display();
    }
#else

    inline void loop(bool force = false) {}

    inline void drawLogo(uint16_t top) {
        File f = LittleFS.open("/images/logo300x118.png", "r");
        if (!f) return;
        size_t   size = f.size();
        uint8_t* buf = (uint8_t*)ps_malloc(size);
        if (!buf) {
            f.close();
            return;
        }
        f.read(buf, size);
        f.close();
        drawPng(buf, size, 0, top, 0, 0, 0, 0, 1.0f, 1.0f, datum_t::top_center);
        free(buf);
    }
#endif
    void  flip();
    void  invert();
    void  sleep();
    void  wake();
    void  setScrollId(void* scrollid) { _scrollid = scrollid; }
    void* getScrollId() { return _scrollid; }

    inline void writePixel(int16_t x, int16_t y, uint16_t color) {
        if (_clipping) {
            if ((x < _cliparea.left) || (x > _cliparea.left + _cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height)) return;
        }
        this->writePixel(x, y, color);
    }
    inline void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        if (_clipping) {
            if ((x < _cliparea.left) || (x >= _cliparea.left + _cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height)) return;
        }
        this->writeFillRect(x, y, w, h, color);
    }

    inline void setClipping(clipArea ca) {
        _cliparea = ca;
        _clipping = true;
    }
    inline void clearClipping() { _clipping = false; }

  private:
    bool         _clipping;
    clipArea     _cliparea;
    void*        _scrollid;
    LGFX_Sprite* _bootSpr = nullptr;
};

extern DspCore dsp;
