#pragma once

#include <FS.h>
#include <LittleFS.h>

#ifndef LGFX_WIDTH
#define LGFX_WIDTH 240
#endif

#ifndef LGFX_HEIGHT
#define LGFX_HEIGHT 320
#endif

#ifndef LGFX_ROTATION
#define LGFX_ROTATION 0
#endif

#include "lgfx/lgfx_base.h"

class LGFX_ST7789 : public LGFX_Base<lgfx::Panel_ST7789> {
	public:
	LGFX_ST7789() : LGFX_Base(LGFX_WIDTH, LGFX_HEIGHT, LGFX_ROTATION) {}
};

typedef LGFX_ST7789 yoDisplay;
typedef lgfx::LGFX_Sprite Canvas;

#define LOGO_PATH "/images/logo200x79.png"
#include "tools/commongfx.h"
#include "conf/conf_320x240.h"

#define ST7789_SLPIN   0x10
#define ST7789_SLPOUT  0x11
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON  0x29
