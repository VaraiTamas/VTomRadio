#    include "../../core/config.h"
#    include "../display_select.h"
#    include "volumeWidget.h"
#    include "../../core/fonts.h"

/*-------------------------------------- VolumeWidget ------------------------------------------*/
VolumeWidget::VolumeWidget(yoDisplay* dsp, const VolumeWidgetConfig& conf) : _dsp(dsp), _spr(dsp) {
    _conf = conf;
    _volume = -1;

    // Widget base init (FONTOS!)
    Widget::init({_conf.left, _conf.top, 0, WA_LEFT}, // pozíció
                 0,                                   // fg (nem kell)
                 config.theme.vol_bg                       // bg
    );

    // Sprite létrehozás
    _spr.setPsram(true);
    _spr.setColorDepth(16);
    _spr.createSprite(_conf.width, _conf.height);
}

void VolumeWidget::setVolume(uint8_t vol) {
    if (vol > _conf.segments) vol = _conf.segments;
    if (vol == _volume) return;

    _volume = vol;
    _draw();
}

void VolumeWidget::_draw() {
    if (!_active) return;

    _spr.fillSprite(config.theme.vol_bg);

    // 1. Háttér rajzolása
    if (_conf.radius > 0) {
        _spr.fillRoundRect(0, 0, _conf.width, _conf.height, _conf.radius, config.theme.vol_bg);
    } else {
        _spr.fillRect(0, 0, _conf.width, _conf.height, config.theme.vol_bg);
    }

    // 2. Keret rajzolása
    if (_conf.border > 0) {
        for (uint8_t i = 0; i < _conf.border; i++) {
            if (_conf.radius > 0) {
                _spr.drawRoundRect(i, i, _conf.width - i * 2, _conf.height - i * 2, _conf.radius, config.theme.vol_border);
            } else {
                _spr.drawRect(i, i, _conf.width - i * 2, _conf.height - i * 2, config.theme.vol_border);
            }
        }
    }

    // --- KÖZÉPRE IGAZÍTÁS LOGIKA ---

    // Szegmensek teljes szélessége
    int segmentsW = (_conf.segments * _conf.segWidth) + ((_conf.segments - 1) * _conf.segGap);
    // Teljes tartalom szélessége: Ikon + 4px köz + szegmensek
    int totalContentW = _conf.iconW + 4 + segmentsW;

    // Kezdő X pozíció a sprite-on belül a középre igazításhoz
    int offsetX = (_conf.width - totalContentW) / 2;

    // 3. Ikon és Szegmensek kirajzolása az eltolással
    _drawIcon(offsetX);
    _drawSegments(offsetX + _conf.iconW + 4);

    _spr.pushSprite(_conf.left, _conf.top);
}

// ---------------- ICON ----------------
void VolumeWidget::_drawIcon(int startX) {
    // Függőleges középre igazítás
    int      y = (_conf.height - _conf.iconH) / 2;
    uint16_t c = config.theme.vol_icon;

    // Geometria (arányosítva az iconW/iconH-hoz)
    int boxW = _conf.iconW / 3;
    int boxH = _conf.iconH / 2.3; // Kicsit kisebb mint a tölcsér
    int boxY = y + (_conf.iconH - boxH) / 2;

    // Pontok
    int hornX_start = startX + boxW - 1;
    int hornY_top_start = boxY;
    int hornY_bottom_start = boxY + boxH - 1;

    int hornX_end = startX + _conf.iconW - 1;
    int hornY_top_end = y;
    int hornY_bottom_end = y + _conf.iconH - 1;

    // Rajzolás
    _spr.drawRect(startX, boxY, boxW, boxH, c);                                     // Doboz
    _spr.drawLine(hornX_start, hornY_top_start, hornX_end, hornY_top_end, c);       // Tölcsér felső
    _spr.drawLine(hornX_start, hornY_bottom_start, hornX_end, hornY_bottom_end, c); // Tölcsér alsó
    _spr.drawLine(hornX_end, hornY_top_end, hornX_end, hornY_bottom_end, c);        // Tölcsér záró
}

// ---------------- SEGMENTS ----------------
void VolumeWidget::_drawSegments(int startX) {
    // Függőleges középre igazítás
    int y = (_conf.height - _conf.segHeight) / 2;

    for (uint8_t i = 0; i < _conf.segments; i++) {
        int      x = startX + i * (_conf.segWidth + _conf.segGap);
        uint16_t col;

        if (i < _volume) {
            float ratio = (float)i / _conf.segments;
            if (ratio < 0.6)
                col = config.theme.vol_low;
            else if (ratio < 0.85)
                col = config.theme.vol_mid;
            else
                col = config.theme.vol_high;
        } else {
            col = config.theme.vol_inactive;
        }

        _spr.fillRect(x, y, _conf.segWidth, _conf.segHeight, col);
    }
}