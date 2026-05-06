#include "../../core/config.h"
#include "../display_select.h"
#include "../../core/network.h"
#include "../tools/language.h"
#include "clockWidget.h"
#include "../../core/fonts.h"

#ifndef CLOCK_WIDGET_SEC_DEBUG
#define CLOCK_WIDGET_SEC_DEBUG 0
#endif

void ClockWidget::init(WidgetConfig clockConf, uint16_t fgcolor, uint16_t bgcolor) {

    _clockConf = clockConf;

    Widget::init(_clockConf, fgcolor, bgcolor);

    _secTopSpace = 10;
    _space = 4;

    _calcSize();
    _getTimeBounds();
    _begin();
}

void ClockWidget::_begin() {
    if (!_spr) { _spr = new LGFX_Sprite(&dsp); }
    _spr->setColorDepth(16);
    _spr->setPsram(true);
    if (_spr) {
        _spr->createSprite(_clockwidth, _clockheight);
        _spr->fillSprite(config.theme.background);
        _spr->setTextDatum(lgfx::top_left);
    }
}

void ClockWidget::_calcSize() {

    if (!_spr) { _spr = new LGFX_Sprite(&dsp); }
    _spr->setColorDepth(16);

    auto measureClockStyle = [this](uint8_t* mainFont, uint8_t* secFont, uint16_t& wTime, uint16_t& hTime, uint16_t& wSec, uint16_t& hSec) {
        if (mainFont) {
            _spr->loadFont(mainFont);
            _spr->setTextSize(1);
        } else {
            _spr->unloadFont();
            _spr->setFont(nullptr);
            _spr->setTextSize(4);
        }
        wTime = _spr->textWidth("88:88");
        hTime = _spr->fontHeight();

        if (secFont) {
            _spr->loadFont(secFont);
            _spr->setTextSize(1);
        } else {
            _spr->unloadFont();
            _spr->setFont(nullptr);
            _spr->setTextSize(3);
        }
        wSec = _spr->textWidth("88");
        hSec = _spr->fontHeight();
    };

    uint8_t* digiMain = nullptr;
    uint8_t* digiSec = nullptr;
    uint8_t* calMain = nullptr;
    uint8_t* calSec = nullptr;
    getClockFontStylePointers(CLOCKFONT_STYLE_DIGI7, &digiMain, &digiSec);
    getClockFontStylePointers(CLOCKFONT_STYLE_CALIBRI, &calMain, &calSec);

    uint16_t wTimeDigi = 0, hTimeDigi = 0, wSecDigi = 0, hSecDigi = 0;
    uint16_t wTimeCalibri = 0, hTimeCalibri = 0, wSecCalibri = 0, hSecCalibri = 0;
    measureClockStyle(digiMain, digiSec, wTimeDigi, hTimeDigi, wSecDigi, hSecDigi);
    measureClockStyle(calMain, calSec, wTimeCalibri, hTimeCalibri, wSecCalibri, hSecCalibri);

    uint16_t w_time = max<uint16_t>(wTimeDigi, wTimeCalibri);
    uint16_t h_time = max<uint16_t>(hTimeDigi, hTimeCalibri);
    uint16_t w_sec = max<uint16_t>(wSecDigi, wSecCalibri);
    _secHeight = max<uint16_t>(hSecDigi, hSecCalibri);

    uint16_t w_right = w_sec;

    if (config.store.clockAmPmStyle) {
        if (font_vlw_22) {
            _spr->loadFont(font_vlw_22);
            _spr->setTextSize(1);
        } else {
            _spr->unloadFont();
            _spr->setFont(nullptr);
            _spr->setTextSize(1);
        }

        uint16_t w_ampm = _spr->textWidth("PM");
        if (w_ampm > w_right) { w_right = w_ampm; }
    }

    // --- layout ---
    uint16_t divider = 5;

    _clockwidth = w_time + _space + divider + w_right;
    _clockheight = h_time;
}

bool ClockWidget::_syncLayoutIfNeeded(bool forceRedraw) {
    const bool fontOrModeChanged = (_lastMainFont != font_vlw_clock) || (_lastSecFont != font_vlw_clock_sec) || (_lastClockFontStyle != config.store.clockFontStyle) ||
                                   (_lastClockAmPmStyle != config.store.clockAmPmStyle);
    const bool missingSprite = !_spr || !_spr->getBuffer();

    if (!forceRedraw && !fontOrModeChanged && !missingSprite) {
        return false;
    }

    _calcSize();
    _getTimeBounds();

    if (!_spr) {
        _spr = new LGFX_Sprite(&dsp);
    }
    _spr->setColorDepth(16);
    _spr->setPsram(true);

    const bool sizeMismatch = !_spr->getBuffer() || _spr->width() != _clockwidth || _spr->height() != _clockheight;
    if (sizeMismatch) {
        if (_spr->getBuffer()) { _spr->deleteSprite(); }
        _spr->createSprite(_clockwidth, _clockheight);
    }

    _spr->fillSprite(config.theme.background);
    _spr->setTextDatum(lgfx::top_left);

    // Date sprite height depends on active fonts, so drop cached buffer after style switches.
    if (_dateSpr && _dateSpr->getBuffer()) {
        _dateSpr->deleteSprite();
    }

    _lastMainFont = font_vlw_clock;
    _lastSecFont = font_vlw_clock_sec;
    _lastClockFontStyle = config.store.clockFontStyle;
    _lastClockAmPmStyle = config.store.clockAmPmStyle;
    _lastRenderedSecond = -1;

    return true;
}

void ClockWidget::_captureTimeSnapshot() {
    network_get_timeinfo_snapshot(&_drawTimeinfo);
}

bool ClockWidget::_getTime() {
    if (config.store.clockAmPmStyle) {
        strftime(_timebuffer, sizeof(_timebuffer), "%I:%M", &_drawTimeinfo);
        if (_timebuffer[0] == '0') {
            _timebuffer[0] = ' '; // Ha az eslő számjegy 0 kicseréli szóközre (azonos karakterszélesség szükséges)
        }
    } else {
        strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &_drawTimeinfo);
    }
    const bool hasValidTime = _drawTimeinfo.tm_year > 100;
    bool       ret = (hasValidTime && _drawTimeinfo.tm_sec == 0) || _forceflag != _drawTimeinfo.tm_year;
    _forceflag = _drawTimeinfo.tm_year;
    return ret;
}

uint16_t ClockWidget::_top() {
    if (_spr && _spr->getBuffer()) return 0; // A Sprite tetejére rajzolunk
    return clockConf.top;                    // Ha nincs Sprite, marad az eredeti
}

uint16_t ClockWidget::_left() {
    if (_spr && _spr->getBuffer()) return 0;
    return _clockConf.left;
}

void ClockWidget::_getTimeBounds() {
    switch (_config.align) {
        case WA_LEFT: _clockleft = _clockConf.left; break;
        case WA_RIGHT: _clockleft = dsp.width() - _clockwidth - _clockConf.left; break;
        default: _clockleft = (dsp.width() / 2 - _clockwidth / 2) + _clockConf.left; break;
    }
    // ❗ FONT-FÜGGŐ MÉRÉS TILOS ITT
    _dotsleft = 0;
}

#if DSP_MODEL == DSP_SSD1322
void ClockWidget::_drawShortDateSSD1322() {
    if (config.isScreensaver) { return; }
    // ⬅️ DÁTUM ELŐÁLLÍTÁSA KÖZÖS HELYEN
    _formatDate(); // _tmp -t tölti fel!
    WidgetConfig dc;
    memcpy_P(&dc, &dateConf, sizeof(WidgetConfig));
    // ===== FIX: 5x7 FONT MÉRETEK =====
    constexpr uint8_t  TS = 1;
    constexpr uint16_t H = CHARHEIGHT * TS;
    uint16_t           dateWidgetWidth = dsp.width() - dc.left;
    dsp.fillRect(dc.left, dc.top, dateWidgetWidth, H, config.theme.background);
    dsp.setFont(nullptr);
    dsp.setTextSize(TS);
    dsp.setTextColor(config.theme.date, config.theme.background); // 0x8410
    // ===== SZÉLESSÉG SZÁMÍTÁS (5x7!) =====
    uint16_t w = strlen(_tmp) * CHARWIDTH * TS;
    uint16_t x;
    switch (dc.align) {
        case WA_CENTER: x = dsp.width() - w - (dateWidgetWidth - w) / 2; break;
        case WA_RIGHT: x = dsp.width() - w; break;
        default: x = dc.left; break;
    }
    // ===== RAJZOLÁS =====
    dsp.setCursor(x, dc.top);
    dsp.print(_tmp);
}
#endif

void ClockWidget::_printClock(bool redraw) {
    if (!_spr || !_spr->getBuffer()) return;

    auto applyMainClockFont = [this]() {
        if (font_vlw_clock) {
            _spr->loadFont(font_vlw_clock);
            _spr->setTextSize(1);
        } else {
            _spr->unloadFont();
            _spr->setFont(nullptr);
            _spr->setTextSize(4);
        }
    };

    auto applySecClockFont = [this]() {
        if (font_vlw_clock_sec) {
            _spr->loadFont(font_vlw_clock_sec);
            _spr->setTextSize(1);
        } else {
            _spr->unloadFont();
            _spr->setFont(nullptr);
            _spr->setTextSize(3);
        }
    };

    auto applyAmPmFont = [this]() {
        if (font_vlw_22) {
            _spr->loadFont(font_vlw_22);
            _spr->setTextSize(1);
        } else {
            _spr->unloadFont();
            _spr->setFont(nullptr);
            _spr->setTextSize(1);
        }
    };

    auto getAmPmSplitY = [this]() -> uint16_t {
        uint16_t splitY = _timeheight / 2;
        if (config.store.clockAmPmStyle) {
            uint16_t shifted = splitY + 7;
            if (_timeheight > 0 && shifted >= _timeheight) shifted = _timeheight - 1;
            splitY = shifted;
        }
        return splitY;
    };

    uint16_t rightBlockWidth = 0;

    // ------------------------------------------------------------
    // 1) Fő idő (óra:perc)
    // ------------------------------------------------------------
    if (redraw) {
        _clearClock();
        _getTimeBounds();
        applyMainClockFont();
        _timewidth = _spr->textWidth(_timebuffer);
        _timeheight = _spr->fontHeight();
        const uint16_t timeBlockW = _spr->textWidth("88:88");
        int16_t        timeX = (int16_t)timeBlockW - (int16_t)_timewidth;
        if (timeX < 0) timeX = 0;

        // --- pontos ':' pozíció számítás a ténylegesen kirajzolt időből ---
        char hourPart[3] = { _timebuffer[0], _timebuffer[1], '\0' };
        _dotsleft = timeX + _spr->textWidth(hourPart);
        _dotswidth = _spr->textWidth(":");

        if (config.store.clockFontStyle == CLOCKFONT_STYLE_DIGI7 && config.store.clockFontMono) {
            const char* ghost = config.store.clockAmPmStyle ? " 8:88" : "88:88";
            int16_t     ghostX = (int16_t)timeBlockW - (int16_t)_spr->textWidth(ghost);
            if (ghostX < 0) ghostX = 0;
            _spr->setTextColor(config.theme.clockbg, config.theme.background);
            _spr->setCursor(ghostX, 0);
            _spr->print(ghost);
            _spr->setTextColor(config.theme.clock);
        } else {
            _spr->setTextColor(config.theme.clock, config.theme.background);
        }

        _spr->setCursor(timeX, 0);
        _spr->print(_timebuffer);

        // --------------------------------------------------------
        // 2) Elválasztó vonalak + jobb oldali blokk
        // --------------------------------------------------------
        _linesleft = timeBlockW + _space;
        rightBlockWidth = _clockwidth - _linesleft;

        // Függőleges vonal
        _spr->drawFastVLine(_linesleft, 0, _timeheight, config.theme.div);

        if (config.store.clockAmPmStyle) {
            const uint16_t splitY = getAmPmSplitY();
            _spr->drawFastHLine(_linesleft, splitY, rightBlockWidth, config.theme.div);

            char buf[3];
            strftime(buf, sizeof(buf), "%p", &_drawTimeinfo);

            applyAmPmFont();
            const uint16_t ampmW = _spr->textWidth(buf);
            const uint16_t ampmH = _spr->fontHeight();
            int16_t ampmX = _linesleft + ((int16_t)rightBlockWidth - (int16_t)ampmW) / 2;
            int16_t ampmY = (int16_t)(splitY + 1) + ((int16_t)_timeheight - (int16_t)(splitY + 1) - (int16_t)ampmH) / 2;
            if (ampmY < (int16_t)(splitY + 1)) ampmY = splitY + 1;

            _spr->setTextColor(config.theme.seconds, config.theme.background);
            _spr->setCursor(ampmX, ampmY);
            _spr->print(buf);
        } else {
#if DSP_MODEL == DSP_ILI9341
            constexpr int lineOffset = 17;
#else
            constexpr int lineOffset = 5;
#endif
            _spr->drawFastHLine(_linesleft, _secTopSpace + _secHeight + lineOffset, _clockwidth - _linesleft, config.theme.div);
        }

        // --------------------------------------------------------
        // 3) DÁTUM külön sprite-ba
        // --------------------------------------------------------
#ifndef HIDE_DATE
        if (!config.isScreensaver) {
            _formatDate();

            if (!_dateSpr) {
                _dateSpr = new LGFX_Sprite(&dsp);
                _dateSpr->setColorDepth(16);
                _dateSpr->setPsram(true);
            }

            memcpy_P(&_dateConf, &dateConf, sizeof(WidgetConfig));

            // FONT kiválasztás
            if (font_vlw_20) {
                _dateSpr->loadFont(font_vlw_20);
            } else {
                _dateSpr->unloadFont();
                _dateSpr->setFont(nullptr);
                _dateSpr->setTextSize(2);
            }

            strlcpy(_datebuf, _tmp, sizeof(_datebuf));

            uint16_t h = _dateSpr->fontHeight();

            // Sprite létrehozás / újraméretezés szélesség az idő szélessége, magasság a font magassága
            if (!_dateSpr->getBuffer() || _dateSpr->width() != _clockwidth || _dateSpr->height() != h) {

                if (_dateSpr->getBuffer()) _dateSpr->deleteSprite();
                _dateSpr->createSprite(_clockwidth, h);
            }

            _dateSpr->fillSprite(config.theme.background);
            _dateSpr->setTextDatum(lgfx::top_right); // A szöveg jobb felső sarka lesz az igazítási pont
            _dateSpr->setTextColor(config.theme.date, config.theme.background);
            _dateSpr->drawString(_datebuf, _dateSpr->width(), 0);

            // jobb igazítás kijelzőn
            uint16_t x = dsp.width() - _clockwidth - _dateConf.left;
            uint16_t y = _dateConf.top;

            _dateSpr->pushSprite(x, y);
            _dateSpr->setTextDatum(lgfx::top_left); // Visszaállítjuk a fő sprite-ot balra igazításra
        }
#endif
    }

    // ------------------------------------------------------------
    // 4) MÁSODPERCEK
    // ------------------------------------------------------------

    const int  currentSecond = _drawTimeinfo.tm_sec;
    const bool showDots = (currentSecond % 2) == 0;
    if (!redraw && _lastRenderedSecond == currentSecond && _lastRenderedDots == showDots) {
#if CLOCK_WIDGET_SEC_DEBUG
        if (_drawTimeinfo.tm_year > 100) {
            Serial.printf("[CLK SKIP] ms=%lu snap=%02d:%02d:%02d last=%d dots=%d redraw=%d\n", millis(), _drawTimeinfo.tm_hour, _drawTimeinfo.tm_min, _drawTimeinfo.tm_sec,
                          _lastRenderedSecond, (int)_lastRenderedDots, (int)redraw);
        }
#endif
        return;
    }

    applySecClockFont();

    sprintf(_tmp, "%02d", currentSecond);
    const uint16_t secW = _spr->textWidth(_tmp);
    uint16_t       secH = _spr->fontHeight();

    uint16_t leftSec;
    uint16_t secTop = _secTopSpace;

    if (config.store.clockAmPmStyle) {
        const uint16_t splitY = getAmPmSplitY();
        if (!rightBlockWidth) { rightBlockWidth = _clockwidth - _linesleft; }
        leftSec = _linesleft + ((int16_t)rightBlockWidth - (int16_t)secW) / 2;
        leftSec += 2;
        secTop = ((int16_t)splitY - (int16_t)secH) / 2;
        if ((int16_t)secTop < 0) secTop = 0;
    } else {
#if DSP_MODEL == DSP_ILI9341
        secTop = 38;
#else
        secTop = _secTopSpace;
#endif
        leftSec = _linesleft + 3;
    }

    int16_t  secClearX = _linesleft + 1;
    uint16_t secClearW = (_clockwidth > _linesleft + 1) ? (_clockwidth - _linesleft - 1) : 0;
    uint16_t secClearY = secTop;
    uint16_t secClearH = secH;

    if (config.store.clockAmPmStyle) {
        const uint16_t splitY = getAmPmSplitY();
        secClearY = 0;
        secClearH = splitY;
    }

    if (secClearX < 0) secClearX = 0;
    if ((uint16_t)secClearX + secClearW > _clockwidth) secClearW = _clockwidth - (uint16_t)secClearX;
    if (secClearY >= _clockheight) secClearY = _clockheight - 1;
    if (secClearY + secClearH > _clockheight) secClearH = _clockheight - secClearY;
    _spr->fillRect((uint16_t)secClearX, secClearY, secClearW, secClearH, config.theme.background);

    if (config.store.clockFontStyle == CLOCKFONT_STYLE_DIGI7 && config.store.clockFontMono) {
        uint16_t ghostColor = config.theme.clockbg;
        if (ghostColor == config.theme.background) {
            // Use a dimmed seconds color when clock shadow equals background.
            ghostColor = (config.theme.seconds & 0xF7DE) >> 1;
            if (ghostColor == config.theme.background) {
                ghostColor = config.theme.seconds;
            }
        }
        _spr->setTextColor(ghostColor, config.theme.background);
        _spr->setCursor(leftSec, secTop);
        _spr->print("88");
        _spr->setTextColor(config.theme.seconds);
    } else {
        _spr->setTextColor(config.theme.seconds, config.theme.background);
    }

    _spr->setCursor(leftSec, secTop);
    _spr->print(_tmp);

    // ------------------------------------------------------------
    // 5) Villogó kettőspont
    // ------------------------------------------------------------
    applyMainClockFont();

    _spr->setTextColor(showDots ? config.theme.clock : config.theme.background, config.theme.background);

    if (!showDots) {
        _spr->fillRect(_dotsleft, 0, _dotswidth, _timeheight, config.theme.background);
    } else {
        _spr->setCursor(_dotsleft, 0);
        _spr->print(":");
    }
    // ------------------------------------------------------------
    // 6) Fő sprite kirajzolása
    // ------------------------------------------------------------
    _spr->pushSprite(_clockleft, _config.top);
    _lastRenderedSecond = currentSecond;
    _lastRenderedDots = showDots;
#if CLOCK_WIDGET_SEC_DEBUG
    tm netDbg{};
    network_get_timeinfo_snapshot(&netDbg);
    Serial.printf("[CLK DRAW] ms=%lu snap=%02d:%02d:%02d net=%02d:%02d:%02d redraw=%d\n", millis(), _drawTimeinfo.tm_hour, _drawTimeinfo.tm_min, _drawTimeinfo.tm_sec, netDbg.tm_hour,
                  netDbg.tm_min, netDbg.tm_sec, (int)redraw);
#endif
}

void ClockWidget::_formatDate() {
#if defined(DSP_OLED) && (DSP_MODEL == DSP_SSD1322)
    // ===== SSD1322: rövid numerikus dátum, futásidőben kiválasztható formátum =====
    switch (config.store.dateFormat) {
        case 0:  snprintf(_tmp, sizeof(_tmp), "%04d.%02d.%02d", _drawTimeinfo.tm_year + 1900, _drawTimeinfo.tm_mon + 1, _drawTimeinfo.tm_mday); break; // HU: YYYY.MM.DD
        case 1:  snprintf(_tmp, sizeof(_tmp), "%02d/%02d/%04d", _drawTimeinfo.tm_mon + 1, _drawTimeinfo.tm_mday, _drawTimeinfo.tm_year + 1900); break;   // EN: MM/DD/YYYY
        case 2:  snprintf(_tmp, sizeof(_tmp), "%02d-%02d-%04d", _drawTimeinfo.tm_mday, _drawTimeinfo.tm_mon + 1, _drawTimeinfo.tm_year + 1900); break;   // NL: DD-MM-YYYY
        case 3:  snprintf(_tmp, sizeof(_tmp), "%02d.%02d.%04d", _drawTimeinfo.tm_mday, _drawTimeinfo.tm_mon + 1, _drawTimeinfo.tm_year + 1900); break;   // PL/DE: DD.MM.YYYY
        case 4:  snprintf(_tmp, sizeof(_tmp), "%02d/%02d/%04d", _drawTimeinfo.tm_mday, _drawTimeinfo.tm_mon + 1, _drawTimeinfo.tm_year + 1900); break;   // ES/GR: DD/MM/YYYY
        default: snprintf(_tmp, sizeof(_tmp), "%04d-%02d-%02d", _drawTimeinfo.tm_year + 1900, _drawTimeinfo.tm_mon + 1, _drawTimeinfo.tm_mday); break;   // ISO fallback
    }
    return;
#else
    // ===== MINDEN MÁS KIJELZŐ: hosszú, szöveges forma, futásidőben kiválasztható =====
    switch (config.store.dateFormat) {
        case 0:  sprintf(_tmp, "%d. %s %2d. %s",   _drawTimeinfo.tm_year + 1900, LANG::mnths[_drawTimeinfo.tm_mon], _drawTimeinfo.tm_mday, LANG::dowf[_drawTimeinfo.tm_wday]); break; // HU: YYYY. MMM DD. DOW
        case 1:  sprintf(_tmp, "%2d %s %d",         _drawTimeinfo.tm_mday, LANG::mnths[_drawTimeinfo.tm_mon], _drawTimeinfo.tm_year + 1900); break;                                       // EN/RU: DD MMM YYYY
        case 2:  sprintf(_tmp, "%s %2d %s %d",      LANG::dowf[_drawTimeinfo.tm_wday], _drawTimeinfo.tm_mday, LANG::mnths[_drawTimeinfo.tm_mon], _drawTimeinfo.tm_year + 1900); break; // NL: DOW DD MMM YYYY
        case 3:  sprintf(_tmp, "%s - %02d %s %04d", LANG::dowf[_drawTimeinfo.tm_wday], _drawTimeinfo.tm_mday, LANG::mnths[_drawTimeinfo.tm_mon], _drawTimeinfo.tm_year + 1900); break; // PL: DOW - DD MMM YYYY
        default: sprintf(_tmp, "%s, %02d. %s %d",   LANG::dowf[_drawTimeinfo.tm_wday], _drawTimeinfo.tm_mday, LANG::mnths[_drawTimeinfo.tm_mon], _drawTimeinfo.tm_year + 1900); break; // DE/SK/UA/ES/GR: DOW, DD. MMM YYYY
    }
#endif
}
void ClockWidget::_clearClock() {
    if (_spr && _spr->getBuffer()) {
        _spr->fillSprite(config.theme.background);
    }
}

void ClockWidget::draw(bool redraw) {
    if (!_active) { return; }
    _captureTimeSnapshot();
    const bool layoutChanged = _syncLayoutIfNeeded(redraw);
    const bool needTimeRedraw = _getTime();
    const bool sameRenderedSecond = (_lastRenderedSecond == _drawTimeinfo.tm_sec) && (_lastRenderedDots == ((_drawTimeinfo.tm_sec % 2) == 0));
    const bool safeForcedRedraw = redraw && !sameRenderedSecond;
    _printClock(needTimeRedraw || safeForcedRedraw || layoutChanged);
}

void ClockWidget::_draw() {
    if (!_active) { return; }
    _captureTimeSnapshot();
    _syncLayoutIfNeeded(true);
    _printClock(true);
}

void ClockWidget::_reset() {
    if (_spr) {
        if (_spr->getBuffer()) { _spr->deleteSprite(); }
        _getTimeBounds();
        _begin();
    }
}

void ClockWidget::_clear() {
    _clearClock();
    if (_spr && _spr->getBuffer()) _spr->pushSprite(_clockleft, _config.top);
}
