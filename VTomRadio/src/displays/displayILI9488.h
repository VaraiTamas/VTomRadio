#pragma once

#include <FS.h>
#include <LittleFS.h>
// ... Csak ezután jöhet a LovyanGFX include ...
#include "lgfx/lgfx_ili9488.h" // LGFX_ILI9488 osztály, amely a LovyanGFX könyvtár egy konkrét panel meghajtója az ILI9488-hoz. Ez a fájl tartalmazza a meghajtó implementációját és konfigurációját az ILI9488 kijelzőhöz.

typedef LGFX_ILI9488 yoDisplay; // DspCore osztályban a yoDisplay typedef LGFX_ILI9488, ami a LovyanGFX könyvtár egy konkrét panel meghajtója az ILI9488-hoz. Ez azt jelenti, hogy a DspCore osztály a
                                // LGFX_ILI9488 meghajtóval fog működni, amely támogatja az ILI9488 kijelzőt.
typedef lgfx::LGFX_Sprite Canvas;

#include "tools/commongfx.h"

#include "conf/displayILI9488conf.h"

#define ILI9488_SLPIN   0x10
#define ILI9488_SLPOUT  0x11
#define ILI9488_DISPOFF 0x28
#define ILI9488_DISPON  0x29
