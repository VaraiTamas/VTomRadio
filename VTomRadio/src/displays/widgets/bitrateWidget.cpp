#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include "../../core/config.h"
#    include "../display_select.h"
#    include "widgets.h"
#    include "../../core/fonts.h"

BitrateWidget::~BitrateWidget() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }
}

void BitrateWidget::init(BitrateBoxConfig boxconf, uint16_t fgcolor, uint16_t bgcolor) {
    Widget::init({boxconf.left, boxconf.top, 0, boxconf.align}, fgcolor, bgcolor);

    _box = boxconf;

    _bitrate = 0;
    _format = BF_UNKNOWN;

    memset(_buf, 0, sizeof(_buf));

    // sprite NULLÁZÁS BIZTOSRA
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }
}

void BitrateWidget::_ensureSprite() {
    bool flat = _isFlat();

    uint16_t w = flat ? _box.dimension * 2 : _box.dimension;
    uint16_t h = flat ? _box.dimension / 2 : _box.dimension;

    if (_spr && (_spr->width() == w) && (_spr->height() == h)) return; // ha van srite és mérete is jó, nem csinál semmit

    if (_spr) {
        _spr->deleteSprite();
    } else {
        _spr = new LGFX_Sprite(&dsp);
        _spr->setColorDepth(16);
        _spr->setPsram(true);
    }

    _spr->createSprite(w, h);
}

bool BitrateWidget::_applyFont() {
    _usingVlw = false;

    if (_box.textsize == 20 && font_vlw_20) {
        _spr->loadFont(font_vlw_20);
        _spr->setTextSize(1);
        _usingVlw = true;
    } else {
        _spr->unloadFont();
        _spr->setFont(nullptr);
        _spr->setTextSize(2);
    }

    return _usingVlw;
}

void BitrateWidget::setBitrate(uint16_t bitrate) {
    _bitrate = bitrate;

    if (_bitrate > 20000) { _bitrate /= 1000; }

    _draw();
}

void BitrateWidget::setFormat(BitrateFormat format) {
    _format = format;
    _draw();
}

void BitrateWidget::_draw() {
    if (!_active) return;

    // Theme can change at runtime from the web editor. Keep widget colors in sync.
    _fgcolor = config.theme.bitrate;
    _bgcolor = config.theme.background;

    if (_format == BF_UNKNOWN || _bitrate == 0) {
        _clear();
       // return;
    }

    _ensureSprite(); // Sprite létrehozása vagy újraméretezése, ha szükséges
    _applyFont();    // Font beállítása (VLW vagy default) a textsize alapján

    _spr->fillSprite(_bgcolor);

    bool flat = _isFlat();

    int w = _spr->width();
    int h = _spr->height();

    // =====================
    // KERET + HÁTTÉR
    // =====================

    if (_box.fill) {
        if (_box.radius > 0) {
            _spr->fillRoundRect(0, 0, w, h, _box.radius, _bgcolor);
        } else {
            _spr->fillRect(0, 0, w, h, _bgcolor);
        }
    }

    if (_box.border > 0) {
        for (uint8_t i = 0; i < _box.border; i++) {
            if (_box.radius > 0) {
                _spr->drawRoundRect(i, i, w - i * 2, h - i * 2, _box.radius, _fgcolor);
            } else {
                _spr->drawRect(i, i, w - i * 2, h - i * 2, _fgcolor);
            }
        }
    }

    // =====================
    // BITRATE STRING
    // =====================

    if (_bitrate == 0) {
        snprintf(_buf, sizeof(_buf), "---");    
    } else if (_bitrate > 0 && _bitrate < 999) {     
        snprintf(_buf, sizeof(_buf), "%d", _bitrate);
    } else {
        float br = (float)_bitrate / 1000;
        snprintf(_buf, sizeof(_buf), "%.1f", br);
    }

    const char* fmt = "";
    switch (_format) {
        case BF_MP3: fmt = "MP3"; break;
        case BF_AAC: fmt = "AAC"; break;
        case BF_FLAC: fmt = "FLAC"; break;
        case BF_OGG: fmt = "OGG"; break;
        case BF_WAV: fmt = "WAV"; break;
        case BF_VOR: fmt = "VOR"; break;
        case BF_OPU: fmt = "OPU"; break;
        default: fmt = "---"; break;
    }

    _spr->setTextDatum(middle_center);
  
    /****** ELRENDEZÉS ******/
    if (!flat) {
        // 🔲 
        // Ha nem flat, akkor négyzetes elrendezés: bitrate fent, formátum lent
        _spr->setTextColor(_fgcolor, _bgcolor);
        _spr->drawString(_buf, w / 2, h / 4);
        int halfH = h / 2;
        if (_box.radius > 0) {
            _spr->fillRoundRect(0, halfH, w, halfH, _box.radius, _fgcolor);
            _spr->fillRect(0, halfH, w, _box.radius, _fgcolor);
        } else {
            _spr->fillRect(0, halfH, w, halfH, _fgcolor);
        }
        _spr->setTextColor(_bgcolor, _fgcolor);
        _spr->drawString(fmt, w / 2, (h * 3) / 4);
    } else {
        // ▭ LAPOS
        // Ha flat, akkor két részre osztott téglalap: bitrate balra, formátum jobbra
        int halfW = w / 2;
        _spr->setTextColor(_fgcolor, _bgcolor);
        _spr->drawString(_buf, halfW / 2, h / 2);
        if (_box.radius > 0) {
            _spr->fillRoundRect(halfW, 0, halfW, h, _box.radius, _fgcolor);
            _spr->fillRect(halfW, 0, _box.radius, h, _fgcolor);

        } else {
            _spr->fillRect(halfW, _box.border, halfW - _box.border, h - _box.border * 2, _fgcolor);
        }
        _spr->setTextColor(_bgcolor, _fgcolor);
        _spr->drawString(fmt, halfW + halfW / 2, h / 2);
    }
    _spr->pushSprite(_config.left, _config.top);
    /*
    //#if // OLED DISPLAY
    // Serial.printf(
    // "widget.cpp--> BITRATE-- left: %d, top: %d, dimension: %d _bitrate: %d, textsize: %d \n ", _config.left, _config.top, _box.dimension, _bitrate, _config.textsize
    // );
    // felső: üres keret (bitrate szám)
    dsp.drawRect(_config.left, _config.top, _box.dimension, _box.dimension, GRAY_5);
    // alsó: kitöltött (formátum)
    dsp.fillRect(_config.left, _config.top + _box.dimension / 2, _box.dimension, _box.dimension / 2, GRAY_5);
    // -------- bitrate szám --------
    dsp.setFont(nullptr);
    dsp.setTextSize(_config.textsize);
    dsp.setTextColor(GRAY_2);

    if (_bitrate < 999) {
        snprintf(_buf, 6, "%d", _bitrate); // Módisítás "bitrate"
    } else {
        float _br = (float)_bitrate / 1000;
        snprintf(_buf, 6, "%.1f", _br);
    }
    uint8_t cw = CHARWIDTH * _config.textsize;
    uint8_t ch = CHARHEIGHT * _config.textsize;
    dsp.setCursor((_config.left + (_box.dimension - strlen(_buf) * cw) / 2) + 1, _config.top + (_box.dimension / 4) - ch / 2 + 1);
    dsp.print(_buf);
    // -------- formátum szöveg --------
    dsp.setTextColor(BLACK);
    const char* fmt = "";
    switch (_format) {
        case BF_MP3: fmt = "MP3"; break;
        case BF_AAC: fmt = "AAC"; break;
        case BF_FLAC: fmt = "FLC"; break;
        case BF_OGG: fmt = "OGG"; break;
        case BF_WAV: fmt = "WAV"; break;
        case BF_VOR: fmt = "VOR"; break;
        case BF_OPU: fmt = "OPU"; break;
        default: return;
    }
    dsp.setCursor((_config.left + (_box.dimension - strlen(fmt) * cw) / 2) + 1, _config.top + (3 * _box.dimension / 4) - ch / 2);
    dsp.print(fmt);
    //#endif
    */
}

inline bool BitrateWidget::_isFlat() {
    return (config.store.nameday && DSP_MODEL != DSP_SSD1322);
}

void BitrateWidget::_clear() {
    if (_isFlat()) {
        dsp.fillRect(_config.left, _config.top, _box.dimension * 2, _box.dimension / 2, config.theme.background); // _bgcolor  próba szürke 0x7BEF
    } else {
        dsp.fillRect(_config.left, _config.top, _box.dimension, _box.dimension, config.theme.background); // _bgcolor  próba szürke 0x7BEF
    }
}

/* Törli mindkét bitratewidget területét és a namedayt is */
void BitrateWidget::clearAll() {
    dsp.fillRect(_config.left, _config.top, _box.dimension * 2, _box.dimension, config.theme.background);  // _bgcolor zöld próba 0xAEE5  
}

void BitrateWidget::refresh() {
    _clear();
    if (_spr) {
        _spr->deleteSprite();
        _spr = nullptr;
    }
    _draw();
}

#endif // DSP_MODEL != DSP_DUMMY