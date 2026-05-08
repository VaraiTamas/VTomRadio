#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include "../display_select.h"
#    include "widgets.h"
#    include "../../core/fonts.h"
#    include <FS.h>
#    include <LittleFS.h>

namespace {
constexpr uint8_t kWifiArcThickness = 2;
constexpr uint8_t kWifiDotRadius = 2;
constexpr uint8_t kWifiArcGap = 3;
} // namespace

WifiWidget::WifiWidget(yoDisplay* display, const WifiWidgetConfig& cfg) : _dsp(display) {
    _cfg = cfg;
    _init();
}

// 🔥 Konstruktor (PROGMEM)
WifiWidget::WifiWidget(yoDisplay* display, const WifiWidgetConfig* cfg) : _dsp(display) {
    if (cfg) {
        // memcpy helyett memcpy_P kell, ha PROGMEM-et használsz!
        memcpy_P(&_cfg, cfg, sizeof(WifiWidgetConfig));
    }
    _init();
}

WifiWidget::~WifiWidget() {
    _freeImageBuffer();
    _deleteSprite();
}

void WifiWidget::_init() {
    Widget::init({_cfg.left, _cfg.top, 0, WA_RIGHT}, config.theme.wifi_border, config.theme.wifi_bg);

    _width = _cfg.width;
    _backMove.width = _cfg.width;

    _ensureSprite();
    _dirty = true;
}

void WifiWidget::_ensureSprite() {
    if (_spr) return;

    _spr = new LGFX_Sprite(_dsp);
    if (!_spr) return;

    _spr->setColorDepth(16);
    _spr->setPsram(true);

    if (_spr->createSprite(_cfg.width, _cfg.height) == nullptr) {
        delete _spr;
        _spr = nullptr;
    }
}

void WifiWidget::_deleteSprite() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }
}

void WifiWidget::_freeImageBuffer() {
    if (_imgBuf) {
        free(_imgBuf);
        _imgBuf = nullptr;
    }
    _imgSize = 0;
    _imgLevel = -1;
}

const char* WifiWidget::_pathForLevel(int8_t level) const {
    switch (level) {
        case 1: return _cfg.image1;
        case 2: return _cfg.image2;
        case 3: return _cfg.image3;
        case 4: return _cfg.image4;
        default: return nullptr;
    }
}

bool WifiWidget::_loadImageForLevel(int8_t level) {
    if (level < 1 || level > 4) return false;

    const char* path = _pathForLevel(level);
    if (!path || path[0] == '\0') return false;

    if (_imgBuf && _imgLevel == level && _imgSize > 0) return true;

    _freeImageBuffer();

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    const size_t size = f.size();
    if (size == 0) {
        f.close();
        return false;
    }

    uint8_t* buf = static_cast<uint8_t*>(ps_malloc(size));
    if (!buf) {
        f.close();
        return false;
    }

    const size_t readBytes = f.read(buf, size);
    f.close();

    if (readBytes != size) {
        free(buf);
        return false;
    }

    _imgBuf = buf;
    _imgSize = size;
    _imgLevel = level;
    return true;
}

bool WifiWidget::_drawImageForStrength() {
    if (_strength < 1 || _strength > 4) return false;
    if (!_loadImageForLevel(_strength)) return false;
    _spr->drawPng(_imgBuf, _imgSize, 0, 0);
    return true;
}

void WifiWidget::_draw() {
    if (!_active || _locked) return;
    if (!_spr) _ensureSprite();
    if (!_spr) return;

    if (!_dirty) {
        _spr->pushSprite(_config.left, _config.top);
        return;
    }

    _spr->fillSprite(config.theme.wifi_bg);

    if (!_drawImageForStrength()) {
        _drawWifi();
    }
    _spr->pushSprite(_config.left, _config.top);

    _dirty = false;
}

void WifiWidget::_clear() {
    _dsp->fillRect(_config.left, _config.top, _cfg.width, _cfg.height, config.theme.wifi_bg);
}

void WifiWidget::_reset() {
    _dirty = true;
}

void WifiWidget::_drawWifi() {
    uint16_t activeColor = _getActiveColor();
    uint16_t inactive = config.theme.wifi_inactive;

    // 1. PONTOS MAGASSÁG SZÁMÍTÁSA
    // Az utolsó ív külső sugara adja meg az ikon tetejét a középponttól (centerY)
    // A pont alja pedig a centerY + dotRadius
    int32_t maxRadius = kWifiDotRadius;
    const uint8_t arcCount = 3;
    for (int i = 0; i < arcCount; i++) { maxRadius += (kWifiArcGap + kWifiArcThickness); }

    int32_t centerX = _cfg.width / 2;

    // KÖZÉPRE IGAZÍTÁS KORREKCIÓVAL
    // A centerY a pont közepe. Úgy toljuk el, hogy a maxRadius (felfelé)
    // és a dotRadius (lefelé) egyensúlyban legyen.
    int32_t centerY = (_cfg.height / 2) + (maxRadius - kWifiDotRadius) / 2;

    // 2. RAJZOLÁS

    // Alsó pont
    uint16_t dotCol = (_strength > 0) ? activeColor : inactive;
    _spr->fillCircle(centerX, centerY, kWifiDotRadius, dotCol);

    // Ívek léptetése
    // Kezdő sugár a pont széle + a rés
    int32_t currentOuterR = kWifiDotRadius;

    for (int i = 0; i < arcCount; i++) {
        // Itt adjuk hozzá a rést ÉS a vastagságot
        currentOuterR += (kWifiArcGap + kWifiArcThickness);

        int32_t innerR = currentOuterR - kWifiArcThickness;

        uint16_t arcCol = ((_strength - 1) > i) ? activeColor : inactive;

        // Rajzolás - start: 225, end: 315 (felfelé néző szimmetrikus ív)
        _spr->fillArc(centerX, centerY, currentOuterR, innerR, 225, 315, arcCol);
    }
}

// Segédfüggvény a vastag ívhez (LovyanGFX fillArc-ot használva)
void WifiWidget::_fillArcThick(int32_t cx, int32_t cy, int32_t outerR, int32_t thickness, int32_t start, int32_t end, uint16_t color) {
    if (thickness <= 0) return;
    // fillArc(x, y, külső_sugár, belső_sugár, kezdő_szög, záró_szög, szín)
    _spr->fillArc(cx, cy, outerR, outerR - thickness, start, end, color);
}

int8_t WifiWidget::_clamp(int v) {
    if (v < 0) return 0;
    if (v > 4) return 4;
    return v;
}

int8_t WifiWidget::_rssiToStrength(int32_t rssi) {
    if (rssi >= -55) return 4; // kiváló
    if (rssi >= -67) return 3; // jó
    if (rssi >= -75) return 2; // gyenge
    if (rssi >= -85) return 1; // nagyon gyenge
    return 1;
}

uint16_t WifiWidget::_getActiveColor() const {
    switch (_strength) {
        case 1: return config.theme.wifi_low;       // piros
        case 2: return config.theme.wifi_low_mid;   // narancs
        case 3: return config.theme.wifi_mid;       // sárga
        case 4: return config.theme.wifi_high;      // zöld
        default: return config.theme.wifi_low; 
    }
}

void WifiWidget::setStrength(int8_t s) {

    s = _clamp(s);
    if (_strength == s) return;
    _strength = s;
    //Serial.printf("_streng %d\n", _strength);
    _dirty = true;
    if (_active && !_locked) _draw();
}

void WifiWidget::setRSSI(int32_t rssi) {
    //Serial.printf("Wifi RSSI: %ld dBm\n", rssi);
    setStrength(_rssiToStrength(rssi));
}

void WifiWidget::invalidate() {
    if (!_active || _locked) { return; }

    // Force redraw with new colors - bypass setStrength() early return
    _dirty = true;
    _draw();
}

#endif