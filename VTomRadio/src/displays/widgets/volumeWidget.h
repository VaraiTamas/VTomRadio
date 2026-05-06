#pragma once

#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include <LovyanGFX.hpp>
#    include "widgetsconfig.h"
#    include "../display_select.h"
#    include "widget.h"

class VolumeWidget : public Widget {
  public:
    VolumeWidget(yoDisplay* dsp, const VolumeWidgetConfig& conf);

    void setVolume(uint8_t vol);

  protected:
    void _draw() override;

  private:
    void _drawIcon(int startX);
    void _drawSegments(int startX);

    yoDisplay*  _dsp;
    LGFX_Sprite _spr;

    VolumeWidgetConfig _conf;

    int8_t _volume;
};
#endif