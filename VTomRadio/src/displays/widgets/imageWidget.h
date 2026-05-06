#pragma once

#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include <LovyanGFX.hpp>
#    include "widgetsconfig.h"
#    include "../display_select.h"
#    include "widget.h"

class ImageWidget : public Widget {
  public:
    ImageWidget(yoDisplay* dsp, const ImageWidgetConfig& cfg, const char* imagePath = nullptr);
    ImageWidget(yoDisplay* dsp, const ImageWidgetConfig* cfg, const char* imagePath = nullptr); // PROGMEM
    ~ImageWidget();

    void        setImagePath(const char* imagePath);
    const char* imagePath() const { return _imagePath; }

  protected:
    void _draw() override;
    void _clear() override;
    void _reset() override;

  private:
    void _init();
    void _ensureSprite();
    void _deleteSprite();
    void _freePngBuffer();
    bool _loadPngFromFs();

    yoDisplay*        _dsp;
    ImageWidgetConfig _cfg;
    LGFX_Sprite*      _spr = nullptr;

    char     _imagePath[160] = {0};
    uint8_t* _pngBuffer = nullptr;
    size_t   _pngSize = 0;
    bool     _dirty = true;
};
#endif
