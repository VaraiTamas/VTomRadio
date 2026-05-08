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

class LGFX_ST7796 : public LGFX_Base<lgfx::Panel_ST7796> {
	public:
	LGFX_ST7796() : LGFX_Base(LGFX_WIDTH, LGFX_HEIGHT, LGFX_ROTATION) {}
};

typedef LGFX_ST7796 yoDisplay;
typedef lgfx::LGFX_Sprite Canvas;

#include "tools/commongfx.h"

#include "conf/conf_480x320.h"

#define ST7796_SLPIN   0x10
#define ST7796_SLPOUT  0x11
#define ST7796_DISPOFF 0x28
#define ST7796_DISPON  0x29
