#pragma once

#include <FS.h>
#include <LittleFS.h>

#ifndef LGFX_WIDTH
#define LGFX_WIDTH 320
#endif

#ifndef LGFX_HEIGHT
#define LGFX_HEIGHT 480
#endif

#ifndef LGFX_ROTATION
#define LGFX_ROTATION 0
#endif

#include "lgfx/lgfx_base.h"

class LGFX_ILI9488 : public LGFX_Base<lgfx::Panel_ILI9488> {
    public:
    LGFX_ILI9488() : LGFX_Base(LGFX_WIDTH, LGFX_HEIGHT, LGFX_ROTATION) {}
};

typedef LGFX_ILI9488 yoDisplay;
typedef lgfx::LGFX_Sprite Canvas;

#define LOGO_PATH "/images/logo300x118.png"
#include "tools/commongfx.h"
#include "conf/conf_480x320.h"

#define ILI9488_SLPIN   0x10
#define ILI9488_SLPOUT  0x11
#define ILI9488_DISPOFF 0x28
#define ILI9488_DISPON  0x29
