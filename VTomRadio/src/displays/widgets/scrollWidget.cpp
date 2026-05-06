#include "../../core/config.h"
#include "../display_select.h"
#include "widgets.h"
#include "../../core/fonts.h"

ScrollWidget::ScrollWidget(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
    init(separator, conf, fgcolor, bgcolor);
}

ScrollWidget::~ScrollWidget() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
    }
    if (_sep) free(_sep);
}

void ScrollWidget::init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {

    TextWidget::init(conf.widget, conf.buffsize, conf.uppercase, fgcolor, bgcolor);

    _sep = (char*)malloc(4);
    if (_sep) snprintf(_sep, 4, " %.*s ", 1, separator);

    _startscrolldelay = conf.startscrolldelay;
    _scrolldelta = conf.scrolldelta;
    _scrolltime = conf.scrolltime;
    _width = conf.width;

    _spr = new LGFX_Sprite(&dsp);
    _spr->setColorDepth(16);
    _spr->setPsram(true);

    // 1. ELŐBB beállítjuk a paramétereket (font betöltése), hogy tudjuk a magasságot
    _setTextParams();

    // 2. Lekérjük a tényleges font magasságot (Roboto36 esetén ez ~40-42 px lesz)
    _textheight = _spr->fontHeight();

    // 3. CSAK MOST hozzuk létre a Sprite-ot a pontos mérettel
    if (!_spr->createSprite(_width, _textheight)) {
        // Ha PSRAM-ba nem sikerült, megpróbáljuk belső RAM-ba
        _spr->setPsram(false);
        _spr->createSprite(_width, _textheight);
    }

    _scrollOffset = 0;
    _doscroll = false;
}

void ScrollWidget::_setTextParams() {
    if (!_spr || _config.textsize == 0) return;
    _spr->setTextColor(_fgcolor, _bgcolor);
    _spr->setTextWrap(false);
    _spr->unloadFont();
    if (_config.textsize == 20 && font_vlw_20) {
        _spr->loadFont(font_vlw_20);
        _spr->setTextSize(1);

    } else if (_config.textsize == 22 && font_vlw_22) {
        _spr->loadFont(font_vlw_22);
        _spr->setTextSize(1);

    } else if (_config.textsize == 24 && font_vlw_24) {
        _spr->loadFont(font_vlw_24);
        _spr->setTextSize(1);
    } else if (_config.textsize == 26 && font_vlw_26) {
        _spr->loadFont(font_vlw_26);
        _spr->setTextSize(1);

    } else if (_config.textsize == 36 && font_vlw_36) {
        _spr->loadFont(font_vlw_36);
        _spr->setTextSize(1);

    } else {
        _spr->setFont(nullptr);
        _spr->setTextSize(2);
    }
}

void ScrollWidget::setFontBySize(uint8_t size) {
    if (!_spr) return;
    _config.textsize = size;
    _setTextParams();
    uint16_t newHeight = _spr->fontHeight();
    if (newHeight != _textheight) {
        _textheight = newHeight;
        _spr->deleteSprite();
        if (!_spr->createSprite(_width, _textheight)) {
            _spr->setPsram(false);
            _spr->createSprite(_width, _textheight);
        }
    }
    _textwidth = _spr->textWidth(_text);
    int sepW = _spr->textWidth(_sep);
    _fullWidth = _textwidth + sepW;
    _doscroll = (_textwidth > _width);
    _scrollOffset = 0;
    _scrollState = SCROLL_WAIT_START;
    _scrolldelay = millis();
    if (_active) { _draw(); }
}

void ScrollWidget::setText(const char* txt) {
    if (!txt) return;
    strlcpy(_text, txt, _buffsize - 1);
    if (strcmp(_oldtext, _text) == 0) return;
    _setTextParams();
    _textwidth = _spr->textWidth(_text);
    int sepW = _spr->textWidth(_sep);
    _fullWidth = _textwidth + sepW;
    _doscroll = (_textwidth > _width);
    _scrollOffset = 0;
    _scrollState = SCROLL_WAIT_START;
    _scrolldelay = millis();
    if (_active) { _draw(); }
    strlcpy(_oldtext, _text, _buffsize);
}

void ScrollWidget::setText(const char* txt, const char* format) {
    if (!txt || !format) return;
    char buf[_buffsize];
    int  n = snprintf(buf, sizeof(buf), format, txt);
    if (n < 0) return;
    buf[sizeof(buf) - 1] = '\0';
    setText(buf);
}

void ScrollWidget::loop() {
    if (_locked || !_active || !_doscroll) return;

    uint32_t now = millis();
    uint32_t waitTime = (_scrollState == SCROLL_WAIT_START) ? _startscrolldelay : _scrolltime;

    if (now - _scrolldelay >= waitTime) {
        _draw();
        // Itt frissítjük az időbélyeget, miután a rajzolás megtörtént
        _scrolldelay = now;
    }
}

void ScrollWidget::setColors(uint16_t fg, uint16_t bg) {
    _fgcolor = fg;
    _bgcolor = bg;
    _setTextParams();
    _draw();
}

// --- TISZTA TÖRLÉS ---
void ScrollWidget::_clear() {
    // Ellenőrizzük, hogy a Sprite létezik-e és van-e mérete
    if (_spr && _spr->width() > 0) {
        _spr->fillSprite(_bgcolor);
        _spr->pushSprite(_config.left, _config.top);
    } else {
        dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
    }
}

void ScrollWidget::_draw() {
    if (!_active || _locked || !_spr || _spr->width() <= 0) return;
    //  --- 1. ESET: NINCS GÖRGETÉS (Rövid szöveg) ---
    if (!_doscroll) {
        _spr->fillSprite(_bgcolor);
        int16_t startX = 0;

        if (_config.align == WA_CENTER) {
            startX = (_width - _textwidth) / 2;
        } else if (_config.align == WA_RIGHT) {
            startX = _width - _textwidth;
        }

        _spr->setCursor(startX, 0);
        _spr->print(_text);
        _spr->pushSprite(_config.left, _config.top);
        return;
    }

    // --- 2. ESET: VÁRAKOZÁS A GÖRGETÉS ELŐTT ---
    if (_scrollState == SCROLL_WAIT_START) {
        // Ha még 0 az offset, rajzoljuk ki az elejét álló helyzetben
        if (_scrollOffset == 0) {
            _spr->fillSprite(_bgcolor);
            _spr->setCursor(0, 0);
            _spr->print(_text);
            _spr->pushSprite(_config.left, _config.top);
        }

        // Időellenőrzés: Csak akkor lépünk tovább, ha letelt a várakozás
        if (millis() - _scrolldelay >= _startscrolldelay) {
            _scrollState = SCROLL_RUN;
            _scrolldelay = millis(); // Reseteljük az időzítőt a folyamatos gördüléshez
        }
        return;
    }

    // --- 3. ESET: AKTÍV GÖRGETÉS (SCROLL_RUN) ---
    if (_scrollState == SCROLL_RUN) {
        // Offset növelése
        _scrollOffset += _scrolldelta;

        // Ha végére értünk a teljes hossznak (szöveg + separator)
        if (_scrollOffset >= _fullWidth) {
            _scrollOffset = 0;
            _scrollState = SCROLL_WAIT_START;
            _scrolldelay = millis(); // Itt indul a 2 másodperces várakozás az elején

            // Kirajzoljuk az alapállapotot, hogy ne maradjon üres vagy félkész a kép
            _spr->fillSprite(_bgcolor);
            _spr->setCursor(0, 0);
            _spr->print(_text);
            _spr->pushSprite(_config.left, _config.top);
            return;
        }

        // Rajzolás folyamata
        _spr->fillSprite(_bgcolor);

        // Első példány kirajzolása (balra csúszik ki)
        _spr->setCursor(-_scrollOffset, 0);
        _spr->print(_text);
        _spr->print(_sep);

        // Második példány kirajzolása (jobbról úszik be)
        // Csak akkor rajzoljuk, ha az első már elkezdett kimenni a képből
        if (_scrollOffset > 0) {
            _spr->setCursor(_fullWidth - _scrollOffset, 0);
            _spr->print(_text);
            _spr->print(_sep);
        }

        _spr->pushSprite(_config.left, _config.top);

        // Frissítjük az utolsó futás idejét a loop() számára
        _scrolldelay = millis();
    }
}
void ScrollWidget::setActive(bool act, bool clr) {
    // 1. Alaphelyzetbe állítjuk a belső változókat, mielőtt bármi történne
    if (act) {
        _scrollOffset = 0;
        _scrollState = SCROLL_WAIT_START;
        _scrolldelay = millis();
        if (_spr && _spr->width() > 0) {
            _spr->fillSprite(_bgcolor); // Megelőzzük a szellemképet
        }
    }

    // 2. Meghívjuk az ősosztály metódusát.
    Widget::setActive(act, clr);
}

void ScrollWidget::_reset() {
    _scrolldelay = millis();
    _scrollOffset = 0;
    _scrollState = SCROLL_WAIT_START;
    if (_spr && _spr->width() > 0) _spr->fillSprite(_bgcolor);
}