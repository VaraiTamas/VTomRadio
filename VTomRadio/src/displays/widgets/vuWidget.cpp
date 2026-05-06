#include "../../core/config.h"
#include "../display_select.h"
#include "vuWidget.h"
#include "../../core/player.h"
#include "../../core/fonts.h"

bool VuWidget::_labelsDrawn = false;

VuWidget::~VuWidget() {
#if !defined(DSP_OLED)
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }
#endif
}

#if !defined(DSP_OLED)
void VuWidget::_recreateSprite() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }

    _spr = new LGFX_Sprite(&dsp);
    if (!_spr) return;

    _spr->setColorDepth(16);
    _spr->setTextDatum(top_left);
    _spr->setPsram(true);

    if (_bidirectional) {
        // ket VU egymas mellett
        _spr->createSprite(_vuConf.width * 2 + _vuConf.space, _vuConf.height);
    } else {
        // ket VU egymas alatt
        _spr->createSprite(_vuConf.width, _vuConf.height * 2 + _vuConf.space);
    }

    if (_spr->width() == 0 || _spr->height() == 0) {
        delete _spr;
        _spr = nullptr;
        return;
    }

    _spr->fillSprite(_bgcolor);
}
#endif

void VuWidget::init(VU_WidgetConfig wconf,
                    uint16_t vumaxcolor, uint16_t vumidcolor,
                    uint16_t vumincolor, uint16_t bgcolor) {
    init(wconf, wconf, vumaxcolor, vumidcolor, vumincolor, bgcolor);
}

void VuWidget::init(VU_WidgetConfig wconf,
                    VU_WidgetConfig wconfBidirectional,
                    uint16_t vumaxcolor, uint16_t vumidcolor,
                    uint16_t vumincolor, uint16_t bgcolor) {
    WidgetConfig wconf_base = {wconf.left, wconf.top, 1, WA_CENTER};
    Widget::init(wconf_base, bgcolor, bgcolor);

    _vuConfNormal = wconf;
    _vuConfBidirectional = wconfBidirectional;
    _vumaxcolor = vumaxcolor;
    _vumidcolor = vumidcolor;
    _vumincolor = vumincolor;

#ifdef DSP_OLED
    _bidirectional = false;
#else
    _bidirectional = config.store.vuBidirectional;
#endif

    _vuConf = _bidirectional ? _vuConfBidirectional : _vuConfNormal;
    _config.left = _vuConf.left;
    _config.top = _vuConf.top;

#ifndef DSP_OLED
    _recreateSprite();
#endif

    _labelsDrawn = false;
}

void VuWidget::switchMode(bool bidirectional) {
#ifdef DSP_OLED
    const bool newMode = false;
#else
    const bool newMode = bidirectional;
#endif

    if (_bidirectional == newMode) { return; }

    _clear();
    _bidirectional = newMode;
    _vuConf = _bidirectional ? _vuConfBidirectional : _vuConfNormal;
    _config.left = _vuConf.left;
    _config.top = _vuConf.top;

#ifndef DSP_OLED
    _recreateSprite();
#endif

    _labelsDrawn = false;
}

/**************************** VU widget DRAW ********************************/
void VuWidget::_draw() {
    if (!_active || _locked) return;
    if (!config.store.vumeter) return;

    // Theme can change at runtime from the web editor. Keep VU colors in sync.
    _vumaxcolor = config.theme.vumax;
    _vumidcolor = config.theme.vumid;
    _vumincolor = config.theme.vumin;
    _bgcolor = config.theme.background;

#ifndef DSP_OLED
    if (!_spr) return;
#endif

    uint16_t        bandColor;
    static uint16_t measLpx = 0;
    static uint16_t measRpx = 0;
    static uint32_t last_draw_time = 0;

    const uint8_t  vu_decay_step = _vuConf.fadespeed;
    const uint16_t dimension = _vuConf.width;
    const uint8_t  refresh_time = 33;
    const bool     vuPeakEnabled = config.store.vuPeak;

    static uint16_t peakL = 0;
    static uint16_t peakR = 0;
    static uint32_t peakL_time = 0;
    static uint32_t peakR_time = 0;
    const uint8_t   peak_decay_step = 1;
    const uint16_t  peak_hold_ms = 200;

    uint32_t now = millis();
    if (last_draw_time + refresh_time > now) return;
    last_draw_time = now;

    uint16_t vulevel = player.getVUlevel();
    uint8_t  vuLeft = (vulevel >> 8) & 0xFF;
    uint8_t  vuRight = vulevel & 0xFF;

    uint16_t maxVU = max(vuLeft, vuRight);
    if (maxVU > config.vuRefLevel) { config.vuRefLevel = maxVU; }
    if (config.vuRefLevel < 50) { config.vuRefLevel = 50; }

    uint16_t vuLpx = map(vuLeft, 0, config.vuRefLevel, 0, _vuConf.width);
    uint16_t vuRpx = map(vuRight, 0, config.vuRefLevel, 0, _vuConf.width);

    bool played = player.isRunning();

    if (played) {
        // BAL csatorna
        if (vuLpx > measLpx) {
            measLpx = vuLpx;
        } else {
            measLpx = (measLpx > vu_decay_step) ? (measLpx - vu_decay_step) : 0;
        }

        // JOBB csatorna
        if (vuRpx > measRpx) {
            measRpx = vuRpx;
        } else {
            measRpx = (measRpx > vu_decay_step) ? (measRpx - vu_decay_step) : 0;
        }

        if (vuPeakEnabled) {
            // BAL peak
            if (measLpx > peakL) {
                peakL = measLpx;
                peakL_time = now;
            } else if (now - peakL_time > peak_hold_ms && peakL > 0) {
                peakL = (peakL > peak_decay_step) ? (peakL - peak_decay_step) : 0;
            }

            // JOBB peak
            if (measRpx > peakR) {
                peakR = measRpx;
                peakR_time = now;
            } else if (now - peakR_time > peak_hold_ms && peakR > 0) {
                peakR = (peakR > peak_decay_step) ? (peakR - peak_decay_step) : 0;
            }
        }
    } else {
        // Nem fut a lejatszas: csak szepen lecseng a kijelzes.
        measLpx = (measLpx > vu_decay_step) ? (measLpx - vu_decay_step) : 0;
        measRpx = (measRpx > vu_decay_step) ? (measRpx - vu_decay_step) : 0;
        if (peakL > 0) peakL = (peakL > peak_decay_step) ? (peakL - peak_decay_step) : 0;
        if (peakR > 0) peakR = (peakR > peak_decay_step) ? (peakR - peak_decay_step) : 0;
    }

/*************************************  A VU savok rajzolasa  ***************************************/
    if (_bidirectional) {
#ifndef DSP_OLED
        _spr->fillSprite(_bgcolor);

        int       center = _spr->width() / 2;
        const int MID_HALF = _vuConf.space / 2;

        int green_end = (_vuConf.width * 65) / 100;
        int yellow_end = (_vuConf.width * 85) / 100;

        uint16_t step = (_vuConf.perheight > 0) ? (dimension / _vuConf.perheight) : 1;
        if (step == 0) step = 1;

        int ledWidth = step - _vuConf.vspace;
        if (ledWidth < 1) ledWidth = 1;

        uint16_t litCountL = measLpx / step;
        uint16_t litCountR = measRpx / step;

        // --- BAL CSATORNA ---
        for (int led = 0; led < (int)litCountL; led++) {
            int x = (center - MID_HALF) - (led * step) - ledWidth;
            if (x < 0) break;

            if (led * (int)step < green_end)
                bandColor = _vumincolor;
            else if (led * (int)step < yellow_end)
                bandColor = _vumidcolor;
            else
                bandColor = _vumaxcolor;

            _spr->fillRect(x, 0, ledWidth, _vuConf.height, bandColor);
        }

        // --- JOBB CSATORNA ---
        for (int led = 0; led < (int)litCountR; led++) {
            int x = (center + MID_HALF) + (led * step);
            if (x + ledWidth > _spr->width()) break;

            if (led * (int)step < green_end)
                bandColor = _vumincolor;
            else if (led * (int)step < yellow_end)
                bandColor = _vumidcolor;
            else
                bandColor = _vumaxcolor;

            _spr->fillRect(x, 0, ledWidth, _vuConf.height, bandColor);
        }

    if (vuPeakEnabled) {
        const uint16_t peak_color = 0xFFFF;
        const uint16_t peak_bright = 0xF7FF;
        const int      peak_width = 1;

        int pxL = (center - MID_HALF) - peakL - peak_width;
        if (pxL < 0) pxL = 0;

        int pxL_shadow = (pxL - 1 < 0) ? 0 : pxL - 1;
        int pxL_w = (pxL_shadow + peak_width + 2 <= _spr->width()) ? (peak_width + 2) : (_spr->width() - pxL_shadow);

        if (pxL_w > 0) { _spr->fillRect(pxL_shadow, 0, pxL_w, _vuConf.height, peak_bright); }
        _spr->fillRect(pxL, 0, peak_width, _vuConf.height, peak_color);

        int pxR = (center + MID_HALF) + peakR;
        int maxX = _spr->width() - peak_width;
        if (pxR > maxX) pxR = maxX;

        int pxR_shadow = (pxR - 1 < 0) ? 0 : pxR - 1;
        int pxR_w = (pxR_shadow + peak_width + 2 <= _spr->width()) ? (peak_width + 2) : (_spr->width() - pxR_shadow);

        if (pxR_w > 0) { _spr->fillRect(pxR_shadow, 0, pxR_w, _vuConf.height, peak_bright); }
        _spr->fillRect(pxR, 0, peak_width, _vuConf.height, peak_color);
    }

        _spr->pushSprite(_vuConf.left, _vuConf.top);
#endif
    } else {
#ifndef DSP_OLED
        _spr->fillSprite(_bgcolor);
#else
        dsp.fillRect(_vuConf.left, _vuConf.top, _vuConf.width, _vuConf.height * 2 + _vuConf.space, BLACK);
#endif

        int green_end = (_vuConf.width * 65) / 100;
        int yellow_end = (_vuConf.width * 85) / 100;

        uint16_t step = (_vuConf.perheight > 0) ? (dimension / _vuConf.perheight) : 1;
        if (step == 0) step = 1;

        int ledWidth = step - _vuConf.vspace;
        if (ledWidth < 1) ledWidth = 1;

        uint16_t litCountL = measLpx / step;
        uint16_t litCountR = measRpx / step;

#ifdef DSP_OLED
        if (played) {
            if (litCountL == 0) litCountL = 1;
            if (litCountR == 0) litCountR = 1;
        }
#endif

        // Bal sav
        for (int led = 0; led < (int)litCountL; led++) {
            int x = led * step;

            if (x < green_end) {
                bandColor = _vumincolor;
            } else if (x < yellow_end) {
                bandColor = _vumidcolor;
            } else {
                bandColor = _vumaxcolor;
            }

#ifndef DSP_OLED
            _spr->fillRect(x, 0, ledWidth, _vuConf.height, bandColor);
#else
            dsp.fillRect(x + _vuConf.left, _vuConf.top, ledWidth, _vuConf.height, bandColor);
#endif
        }

        // Jobb sav
        for (int led = 0; led < (int)litCountR; led++) {
            int x = led * step;

            if (x < green_end) {
                bandColor = _vumincolor;
            } else if (x < yellow_end) {
                bandColor = _vumidcolor;
            } else {
                bandColor = _vumaxcolor;
            }

#ifndef DSP_OLED
            _spr->fillRect(x, _vuConf.height + _vuConf.space, ledWidth, _vuConf.height, bandColor);
#else
            dsp.fillRect(_vuConf.left + x, _vuConf.top + _vuConf.height + _vuConf.space, ledWidth, _vuConf.height, bandColor);
#endif
        }

    if (vuPeakEnabled) {
#    ifndef DSP_OLED
        const uint16_t peak_color = 0xFFFF;
        const uint16_t peak_bright = 0xF7FF;
#    else
        const uint16_t peak_color = 0x000F;
        const uint16_t peak_bright = BLACK;
#    endif
        const int peak_width = 1;

        uint16_t drawPeakL = peakL;
        if (drawPeakL > _vuConf.width - 2) drawPeakL -= 2;

        if (drawPeakL > 1 && drawPeakL <= _vuConf.width) {
#    ifndef DSP_OLED
        _spr->fillRect(drawPeakL - 1, 0, peak_width + 2, _vuConf.height, peak_bright);
        _spr->fillRect(drawPeakL, 0, peak_width, _vuConf.height, peak_color);
#    else
        dsp.fillRect(drawPeakL - 1 + _vuConf.left, _vuConf.top, peak_width + 1, _vuConf.height, peak_bright);
        dsp.fillRect(drawPeakL + _vuConf.left, _vuConf.top, peak_width, _vuConf.height, peak_color);
#    endif
        }

        uint16_t drawPeakR = peakR;
        if (drawPeakR > _vuConf.width - 2) drawPeakR -= 2;

        if (drawPeakR > 1 && drawPeakR <= _vuConf.width) {
#    ifndef DSP_OLED
        _spr->fillRect(drawPeakR - 1, _vuConf.height + _vuConf.space, peak_width + 2, _vuConf.height, peak_bright);
        _spr->fillRect(drawPeakR, _vuConf.height + _vuConf.space, peak_width, _vuConf.height, peak_color);
#    else
        dsp.fillRect(drawPeakR - 1 + _vuConf.left, _vuConf.top + _vuConf.height + _vuConf.space, peak_width + 1, _vuConf.height, peak_bright);
        dsp.fillRect(drawPeakR + _vuConf.left, _vuConf.top + _vuConf.height + _vuConf.space, peak_width, _vuConf.height, peak_color);
#    endif
        }
    }

#ifndef DSP_OLED
        _spr->pushSprite(_vuConf.left, _vuConf.top);
#endif
    }

    /********************************** --- L/R cimek rajzolasa --- **********************************/
    if (played && !_labelsDrawn) {
        if (_bidirectional) {
#ifndef DSP_OLED
            int       sprite_base_x = _vuConf.left;
            int       sprite_center_on_screen = sprite_base_x + (_spr->width() / 2);
            const int MID_HALF = _vuConf.space / 2;

            int label_left_L = (sprite_center_on_screen - MID_HALF) - _vuConf.labelwidth;
            int label_left_R = (sprite_center_on_screen + MID_HALF);
            int labelTop = _vuConf.top - _vuConf.labelheight - 2;

            bool fontLoaded = false;

            if (_vuConf.textsize == 9 && font_vlw_9) {
                if (dsp.loadFont(font_vlw_9)) {
                    fontLoaded = true;
                    dsp.setTextSize(1);
                } else {
                    dsp.setFont(nullptr);
                    dsp.setTextSize(2);
                }
            } else {
                dsp.setFont(nullptr);
                dsp.setTextSize(2);
            }

            int y_center = labelTop + (_vuConf.labelheight / 2);
            if (fontLoaded) {
                y_center += 1;
            }
            dsp.setTextColor(config.theme.vulrtext);
            dsp.setTextDatum(MC_DATUM);

            dsp.drawRect(label_left_L, labelTop, _vuConf.labelwidth, _vuConf.labelheight, config.theme.vulrbox);
            dsp.drawString("L", label_left_L + (_vuConf.labelwidth / 2), y_center);

            dsp.drawRect(label_left_R, labelTop, _vuConf.labelwidth, _vuConf.labelheight, config.theme.vulrbox);
            dsp.drawString("R", label_left_R + (_vuConf.labelwidth / 2), y_center);

            _labelsDrawn = true;

            if (fontLoaded) { dsp.unloadFont(); }
#endif
        } else {
#ifndef DSP_OLED
            int  label_left = _vuConf.left - _vuConf.labelwidth - 4;
            bool fontLoaded = false;

            if (_vuConf.textsize == 9 && font_vlw_9) {
                if (dsp.loadFont(font_vlw_9)) {
                    fontLoaded = true;
                    dsp.setTextSize(1);
                } else {
                    dsp.setFont(nullptr);
                    dsp.setTextSize(2);
                }
            } else {
                dsp.setFont(nullptr);
                dsp.setTextSize(2);
            }

            if (label_left >= 0) {
                int top_L = _vuConf.top - (_vuConf.labelheight - _vuConf.height);
                int top_R = _vuConf.top + _vuConf.height + _vuConf.space;

                dsp.setTextColor(config.theme.vulrtext);
                dsp.setTextDatum(MC_DATUM);

                dsp.drawRect(label_left, top_L, _vuConf.labelwidth, _vuConf.labelheight, config.theme.vulrbox);
                dsp.drawString("L", label_left + _vuConf.labelwidth / 2, top_L + _vuConf.labelheight / 2 + 1);

                dsp.drawRect(label_left, top_R, _vuConf.labelwidth, _vuConf.labelheight, config.theme.vulrbox);
                dsp.drawString("R", label_left + _vuConf.labelwidth / 2, top_R + _vuConf.labelheight / 2 + 1);

                _labelsDrawn = true;
            }

            if (fontLoaded) { dsp.unloadFont(); }
#else
            dsp.setTextColor(0xF);
            dsp.setTextSize(1);
            dsp.setFont(&TinyFont5);

            int text_x = _vuConf.left - 6;
            int text_y_L = _vuConf.top + 3;
            int text_y_R = _vuConf.top + _vuConf.height + _vuConf.space + 6;

            dsp.setCursor(text_x, text_y_L);
            dsp.print("L");

            dsp.setCursor(text_x, text_y_R);
            dsp.print("R");

            dsp.setFont(nullptr);
            _labelsDrawn = true;
#endif
        }
    }
}

void VuWidget::loop() {
    if (_active && !_locked) {
        _draw();
        // Az LR feliratot kulon kell rajzoltatni.
    }
}

void VuWidget::_clear() {
#ifndef DSP_OLED
    int clearW = 0;
    int clearH = 0;

    if (_spr) {
        clearW = _spr->width();
        clearH = _spr->height();
    } else {
        if (_bidirectional) {
            clearW = _vuConf.width * 2 + _vuConf.space;
            clearH = _vuConf.height;
        } else {
            clearW = _vuConf.width;
            clearH = _vuConf.height * 2 + _vuConf.space;
        }
    }

    // ---- VU sav torles ----
    dsp.fillRect(_vuConf.left, _vuConf.top, clearW, clearH, config.theme.background);

    // ---- LABEL torles ----
    if (_bidirectional) {
        int sprite_center_on_screen;
        if (_spr) {
            sprite_center_on_screen = _vuConf.left + (_spr->width() / 2);
        } else {
            sprite_center_on_screen = _vuConf.left + ((_vuConf.width * 2 + _vuConf.space) / 2);
        }

        const int MID_HALF = _vuConf.space / 2;
        int       label_left_L = (sprite_center_on_screen - MID_HALF) - _vuConf.labelwidth;
        int       labelTop = _vuConf.top - _vuConf.labelheight - 2;
        int       total_width = 2 * _vuConf.labelwidth + _vuConf.space;

        dsp.fillRect(label_left_L, labelTop, total_width, _vuConf.labelheight, config.theme.background);
    } else {
        int label_left = _vuConf.left - _vuConf.labelwidth - 4;

        if (label_left >= 0) {
            int top_L = _vuConf.top - (_vuConf.labelheight - _vuConf.height);
            int top_R = _vuConf.top + _vuConf.height + _vuConf.space;
            dsp.fillRect(label_left, top_L, _vuConf.labelwidth, _vuConf.labelheight, config.theme.background);
            dsp.fillRect(label_left, top_R, _vuConf.labelwidth, _vuConf.labelheight, config.theme.background);
        }
    }

#else
    // OLED
    dsp.fillRect(_vuConf.left, _vuConf.top - 6, _vuConf.width, _vuConf.height * 2 + _vuConf.space + 6, config.theme.bitrate);
#endif

    _labelsDrawn = false;
}

void VuWidget::setLabelsDrawn(bool value) {
    _labelsDrawn = value;
}

bool VuWidget::isLabelsDrawn() {
    return _labelsDrawn;
}
