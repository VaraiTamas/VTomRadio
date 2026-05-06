#include "../../core/config.h"
#include "../../core/network.h"
#include "../../core/namedays.h"
#include "../display_select.h"
#include "../tools/language.h"
#include "namedayWidget.h"
#include "../../core/fonts.h"

#ifdef NAMEDAYS_FILE
NamedayWidget::~NamedayWidget() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }
}

void NamedayWidget::init(NamedayWidgetConfig ndwconf, uint16_t fgcolor, uint16_t bgcolor) {

    WidgetConfig wconf{ndwconf.left, ndwconf.top, ndwconf.textsize1, ndwconf.align};

    Widget::init(wconf, fgcolor, bgcolor);
    _namedaywidth = 0;
    _oldnamedaywidth = 0;
    _lastRotation = 0;
    memset(_namedayBuf, 0, sizeof(_namedayBuf));
    memset(_oldNamedayBuf, 0, sizeof(_oldNamedayBuf));
}

void NamedayWidget::_ensureSprite() {
    if (!_spr) {
        _spr = new LGFX_Sprite(&dsp);
        _spr->setColorDepth(16);
        _spr->setPsram(true);
        _spr->setTextDatum(lgfx::top_left);
    }

    memcpy_P(&_namedayConf, &namedayConf, sizeof(NamedayWidgetConfig));

    uint16_t w = _namedayConf.width;
    uint16_t h = _namedayConf.height;

    if (_spr->width() != w || _spr->height() != h) {
        _spr->deleteSprite();
        _spr->createSprite(w, h);
        _spr_w = w;
        _spr_h = h;
    }
}

bool NamedayWidget::_applyFont(uint8_t size) {
    uint8_t* f = nullptr;

    if (size == 18)
        f = font_vlw_18;
    else if (size == 20)
        f = font_vlw_20;

    if (f) {
        _spr->loadFont(f);
        _spr->setTextSize(1);
        return true;
    }

    _spr->unloadFont();
    _spr->setTextSize(2);
    return false;
}

void NamedayWidget::_getNamedayUpper(char* dest, size_t len) {
    tm t{};
    network_get_timeinfo_snapshot(&t);
    const char* nameday = getNameDay(t.tm_mon + 1, t.tm_mday);

    char tmp[32];
    strlcpy(tmp, nameday, sizeof(tmp));

    strlcpy(dest, tmp, len);
}

void NamedayWidget::_printNameday(bool force) {

    if (!config.store.nameday) return;
    if (config.isScreensaver) return;

    _getNamedayUpper(_namedayBuf, sizeof(_namedayBuf));

    if (!force && strcmp(_oldNamedayBuf, _namedayBuf) == 0) { return; }

    _ensureSprite();

    if (!_spr || !_spr->getBuffer()) return;

    _spr->fillSprite(config.theme.background);
    // --- LABEL (font size 18) ---
    _applyFont(_namedayConf.textsize1);

    _spr->setTextColor(config.theme.nameday_label, config.theme.background);
    _spr->setCursor(0, 0);
    _spr->print(nameday_label);

    // --- NAME (font size 20) ---
    _applyFont(_namedayConf.textsize2);

    _spr->setTextColor(config.theme.nameday, config.theme.background);

    int16_t y = 22; // egyszerű fix offset (később finomítható)

    _spr->setCursor(0, y);
    _spr->print(_namedayBuf);

    strlcpy(_oldNamedayBuf, _namedayBuf, sizeof(_oldNamedayBuf));

    // --- KIRAJZOLÁS ---
    memcpy_P(&_namedayConf, &namedayConf, sizeof(NamedayWidgetConfig));

    _spr->pushSprite(_namedayConf.left, _namedayConf.top);
}

void NamedayWidget::clearNameday() {
    memcpy_P(&_namedayConf, &namedayConf, sizeof(NamedayWidgetConfig));

    dsp.fillRect(_namedayConf.left, _namedayConf.top, _spr_w, _spr_h, config.theme.background);
}

void NamedayWidget::draw(bool force) {
    if (!_active) return;
    if (!config.store.nameday) return;

    if (force) {
        _printNameday(true);
        _lastRotation = millis();
        return;
    }

    if (millis() - _lastRotation >= 4000) {
        _printNameday(false);
        _lastRotation = millis();
    }
}

void NamedayWidget::_draw() {
    if (!_active) return;
    _printNameday(true);
}

void NamedayWidget::_clear() {
    clearNameday();
}

#endif // NAMEDAYS_FILE