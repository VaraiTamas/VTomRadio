#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include "../display_select.h"
#    include "imageWidget.h"
#    include <FS.h>
#    include <LittleFS.h>
#    include <cstring>

ImageWidget::ImageWidget(yoDisplay* dsp, const ImageWidgetConfig& cfg, const char* imagePath) : _dsp(dsp), _cfg(cfg) {
    Widget::init({_cfg.left, _cfg.top, 0, WA_LEFT}, 0, config.theme.background);
    _init();
    setImagePath(imagePath);
}

ImageWidget::ImageWidget(yoDisplay* dsp, const ImageWidgetConfig* cfg, const char* imagePath) : _dsp(dsp) {
    memset(&_cfg, 0, sizeof(_cfg));
    if (cfg) {
        memcpy_P(&_cfg, cfg, sizeof(ImageWidgetConfig));
    }
    Widget::init({_cfg.left, _cfg.top, 0, WA_LEFT}, 0, config.theme.background);
    _init();
    setImagePath(imagePath);
}

ImageWidget::~ImageWidget() {
    _freePngBuffer();
    _deleteSprite();
}

void ImageWidget::_init() {
    _width = _cfg.width;
    _backMove.width = _cfg.width;

    _ensureSprite();

    _dirty = true;
}

void ImageWidget::_ensureSprite() {
    if (_spr) return;

    if (_cfg.width == 0 || _cfg.height == 0) return;

    _spr = new LGFX_Sprite(_dsp);
    if (!_spr) return;

    _spr->setColorDepth(16);
    _spr->setPsram(true);

    if (_spr->createSprite(_cfg.width, _cfg.height) == nullptr) {
        delete _spr;
        _spr = nullptr;
    }
}

void ImageWidget::_deleteSprite() {
    if (_spr) {
        _spr->deleteSprite();
        delete _spr;
        _spr = nullptr;
    }
}

void ImageWidget::_freePngBuffer() {
    if (_pngBuffer) {
        free(_pngBuffer);
        _pngBuffer = nullptr;
    }
    _pngSize = 0;
}

bool ImageWidget::_loadPngFromFs() {
    _freePngBuffer();

    if (_imagePath[0] == '\0') return false;

    File f = LittleFS.open(_imagePath, "r");
    if (!f) return false;

    const size_t sz = f.size();
    if (sz == 0) {
        f.close();
        return false;
    }

    uint8_t* buf = static_cast<uint8_t*>(ps_malloc(sz));
    if (!buf) {
        f.close();
        return false;
    }

    const size_t readSz = f.read(buf, sz);
    f.close();

    if (readSz != sz) {
        free(buf);
        return false;
    }

    _pngBuffer = buf;
    _pngSize = sz;
    return true;
}

void ImageWidget::setImagePath(const char* imagePath) {
    if (!imagePath) {
        _imagePath[0] = '\0';
        _freePngBuffer();
        _dirty = true;
        if (_active && !_locked) _draw();
        return;
    }

    if (strncmp(_imagePath, imagePath, sizeof(_imagePath) - 1) == 0) return;

    strncpy(_imagePath, imagePath, sizeof(_imagePath) - 1);
    _imagePath[sizeof(_imagePath) - 1] = '\0';

    _loadPngFromFs();
    _dirty = true;

    if (_active && !_locked) _draw();
}

void ImageWidget::_draw() {
    if (!_active || _locked) return;

    if (!_pngBuffer || _pngSize == 0) {
        if (!_loadPngFromFs()) return;
    }

    if (!_spr) _ensureSprite();
    if (!_spr) return;

    if (_dirty) {
        _spr->fillSprite(_bgcolor);
        _spr->drawPng(_pngBuffer, _pngSize, 0, 0);
        _dirty = false;
    }
    _spr->pushSprite(_config.left, _config.top);
}

void ImageWidget::_clear() {
    if (_cfg.width > 0 && _cfg.height > 0) {
        _dsp->fillRect(_config.left, _config.top, _cfg.width, _cfg.height, _bgcolor);
    }
}

void ImageWidget::_reset() {
    _dirty = true;
}

#endif
