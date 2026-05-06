#include "../../core/config.h"
#include "../display_select.h"
#include "widgets.h"
#include "../../core/fonts.h"

textBoxWidget::~textBoxWidget() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }

    if (_text) {
        free(_text);
        _text = nullptr;
    }

    if (_oldtext) {
        free(_oldtext);
        _oldtext = nullptr;
    }
}

void textBoxWidget::init(textBoxConfig conf, uint16_t buffsize, bool uppercase) {
    _boxconf = conf;

    WidgetConfig wconf = {conf.left, conf.top, conf.textsize, conf.align};

    Widget::init(wconf, _fgcolor, _bgcolor);

    _buffsize = buffsize;
    _uppercase = uppercase;

    _text = (char*)malloc(_buffsize);
    _oldtext = (char*)malloc(_buffsize);

    if (_text) memset(_text, 0, _buffsize);
    if (_oldtext) memset(_oldtext, 0, _buffsize);

    _spr = new LGFX_Sprite(&dsp);
    _spr->setColorDepth(16);
    _spr->setPsram(true);

    _resetFontCache();
}

void textBoxWidget::_resetFontCache() {
    _loadedFontSize = 0xFF;
    _loadedAsVlw = false;
}

void textBoxWidget::_ensureSprite() {
    if (!_spr) return;
    if (_spr->width() != _boxconf.width || _spr->height() != _boxconf.height) {
        _spr->deleteSprite();
        _spr->createSprite(_boxconf.width, _boxconf.height);
        _resetFontCache();
    }
}

bool textBoxWidget::_applyFont() {
    if (!_spr) return false;

    const bool useVlw = (_boxconf.textsize >= 5);

    if (_loadedFontSize == _boxconf.textsize && _loadedAsVlw == useVlw) { return useVlw; }

    _spr->unloadFont();
    _spr->setFont(nullptr);
    _spr->setTextWrap(false);

    if (!useVlw) {
        _spr->setTextSize(_boxconf.textsize);
        _loadedFontSize = _boxconf.textsize;
        _loadedAsVlw = false;
        return false;
    }

    bool ok = false;

    if (_boxconf.textsize == 9 && font_vlw_9) {
        _spr->loadFont(font_vlw_9);
        ok = true;
    } else if (_boxconf.textsize == 12 && font_vlw_12) {
        _spr->loadFont(font_vlw_12);
        ok = true;
    } else if (_boxconf.textsize == 16 && font_vlw_16) {
        _spr->loadFont(font_vlw_16);
        ok = true;
    } else if (_boxconf.textsize == 18 && font_vlw_18) {
        _spr->loadFont(font_vlw_18);
        ok = true;
    } else if (_boxconf.textsize == 20 && font_vlw_20) {
        _spr->loadFont(font_vlw_20);
        ok = true;
    } else if (_boxconf.textsize == 22 && font_vlw_22) {
        _spr->loadFont(font_vlw_22);
        ok = true;
    } else if (_boxconf.textsize == 24 && font_vlw_24) {
        _spr->loadFont(font_vlw_24);
        ok = true;
    } else if (_boxconf.textsize == 26 && font_vlw_26) {
        _spr->loadFont(font_vlw_26);
        ok = true;
    } else if (_boxconf.textsize == 36 && font_vlw_36) {
        _spr->loadFont(font_vlw_36);
        ok = true;
    }

    if (ok) {
        _spr->setTextSize(1); // VLW-nél kötelező
        _loadedFontSize = _boxconf.textsize;
        _loadedAsVlw = true;
        return true;
    }

    // ha nincs ilyen VLW betöltve, essen vissza sima fontra
    _spr->setFont(nullptr);
    _spr->setTextSize(2);
    _loadedFontSize = _boxconf.textsize;
    _loadedAsVlw = false;
    return false;
}

void textBoxWidget::setText(const char* txt) {
    if (!_text || !_oldtext || _buffsize == 0) return;

    strlcpy(_text, txt, _buffsize);

    if (strcmp(_oldtext, _text) == 0) return;

    if (_active) { _draw(); }
}

void textBoxWidget::setText(int val, const char* format) {
    if (!_text || _buffsize == 0) return;

    char buf[_buffsize];
    snprintf(buf, _buffsize, format, val);
    setText(buf);
}

void textBoxWidget::setText(const char* txt, const char* format) {
    if (!_text || _buffsize == 0) return;

    char buf[_buffsize];
    snprintf(buf, _buffsize, format, txt);
    setText(buf);
}

void textBoxWidget::_draw() {
    if (!_active) return;
    if (!_spr) return;
    if (_boxconf.width == 0 || _boxconf.height == 0) return;

    _ensureSprite();
    if (_spr->width() == 0 || _spr->height() == 0) return;

    const bool usingVlw = _applyFont();

    _spr->fillSprite(_bgcolor);

    if (_boxconf.fill) {
        if (_boxconf.radius > 0) {
            _spr->fillRoundRect(0, 0, _boxconf.width, _boxconf.height, _boxconf.radius, _bgcolor);
        } else {
            _spr->fillRect(0, 0, _boxconf.width, _boxconf.height, _bgcolor);
        }
    }

    if (_boxconf.border > 0) {
        for (uint8_t i = 0; i < _boxconf.border; ++i) {
            const int16_t x = i;
            const int16_t y = i;
            const int16_t w = _boxconf.width - (i * 2);
            const int16_t h = _boxconf.height - (i * 2);
            if (w <= 0 || h <= 0) break;

            uint8_t r = (_boxconf.radius > i) ? (_boxconf.radius - i) : 0;

            if (r > 0) {
                uint8_t maxr = (min(w, h)) / 2;
                if (r > maxr) r = maxr;
                _spr->drawRoundRect(x, y, w, h, r, _borderColor);
            } else {
                _spr->drawRect(x, y, w, h, _borderColor);
            }
        }
    }

    _spr->setTextColor(_fgcolor, _bgcolor);
    _spr->setTextWrap(false);

    // 🔥 DATUM RESET (biztonság)
    _spr->setTextDatum(top_left);

    // belső terület
    int16_t innerLeft = _boxconf.paddingX + _boxconf.border;
    int16_t innerTop = _boxconf.paddingY + _boxconf.border;
    int16_t innerRight = _boxconf.width - _boxconf.paddingX - _boxconf.border;
    int16_t innerBottom = _boxconf.height - _boxconf.paddingY - _boxconf.border;

    if (innerRight < innerLeft) innerRight = innerLeft;
    if (innerBottom < innerTop) innerBottom = innerTop;

    int16_t innerW = innerRight - innerLeft;
    int16_t innerH = innerBottom - innerTop;

    // 🔥 KÖZÉP PONT
    int16_t cx = innerLeft + (innerW / 2);
    int16_t cy = innerTop + (innerH / 2);

    // 🔥 IGAZÍTÁS DATUMMAL
    switch (_boxconf.align) {

        case WA_CENTER:
            _spr->setTextDatum(middle_center);
            _spr->drawString(_text, cx, cy);
            break;

        case WA_RIGHT:
            _spr->setTextDatum(middle_right);
            _spr->drawString(_text, innerRight, cy);
            break;

        default:
            _spr->setTextDatum(middle_left);
            _spr->drawString(_text, innerLeft, cy);
            break;
    }

    if (_boxconf.fill) {
        dsp.fillRect(_boxconf.left, _boxconf.top, _boxconf.width, _boxconf.height, _bgcolor);
        _spr->pushSprite(_boxconf.left, _boxconf.top);
    } else {
        _spr->pushSprite(_boxconf.left, _boxconf.top);
    }

    strlcpy(_oldtext, _text, _buffsize);

    // a sprite saját fontját hagyjuk cache-ben,
    // a globális dsp fontállapothoz nem nyúlunk
    (void)usingVlw;
}

void textBoxWidget::_clear() {
    if (_boxconf.width == 0 || _boxconf.height == 0) return;
    dsp.fillRect(_boxconf.left, _boxconf.top, _boxconf.width, _boxconf.height, _bgcolor);
}
