// clang-format off
#pragma once

#include "../../core/config.h"
#include "../../core/display.h"
#include "../widgets/widgets.h"

// ILI9341 display: 320x240 (landscape after rotation)
// Derived from conf_480x320.h, coordinates scaled ~2/3 to fit 320x240.
// Cross-referenced with yoRadio displayILI9341conf_custom.h for widget placement.

#define DSP_WIDTH       320
#define DSP_HEIGHT      240
#define TFT_FRAMEWDT     8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define CLOCK_HEIGHT    70

#if BITRATE_FULL
  #define TITLE_FIX  44
#else
  #define TITLE_FIX   0
#endif
#define bootLogoTop 50

/* SCROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf       PROGMEM = {{ TFT_FRAMEWDT, 0, 26, WA_CENTER    }, 140, true,  MAX_WIDTH, 5000, 3, 30 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT, 33, 16, WA_CENTER   }, 140, true,  MAX_WIDTH, 5000, 3, 30 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT, 52, 16, WA_CENTER   }, 140, false, MAX_WIDTH, 5000, 3, 30 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 112, 18, WA_LEFT    }, 140, false, MAX_WIDTH, 1000, 3, 30 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 3, 30 };
const ScrollConfig weatherConf    PROGMEM = {{ TFT_FRAMEWDT, 75, 16, WA_CENTER }, 300, false, MAX_WIDTH, 0, 3, 60 };

/* BACKGROUNDS  */                        /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 32, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 32, 0, WA_LEFT }, DSP_WIDTH, 1, false };
const FillConfig   volbarConf     PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-6, 0, WA_LEFT }, MAX_WIDTH, 5, true };
const FillConfig   playlBGConf    PROGMEM = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false };
const FillConfig   heapbarConf    PROGMEM = {{ 0, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH, 2, false };

// left, top, width, height, textsize, align, border, radius, fill, paddingX, paddingY
const textBoxConfig bootstrConf   PROGMEM = { 40, 190, 240, 25, 16, WA_CENTER, 1, 4, true, 0, 0 };
const textBoxConfig ipBoxConf     PROGMEM = { TFT_FRAMEWDT, 213, 100, 20, 12, WA_CENTER, 1, 4, true, 0, 0 };
const textBoxConfig chBoxConf     PROGMEM = { 186, 213, 59, 20, 12, WA_CENTER, 1, 4, true, 0, 0 };
const textBoxConfig rssiBoxConf   PROGMEM = { 253, 213, 59, 20, 12, WA_CENTER, 1, 4, true, 0, 0 };

/* WIDGETS  */                            /* { left, top, fontsize, align } */
const WidgetConfig numConf        PROGMEM = { 0, 120, 52, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { TFT_FRAMEWDT, 66, 2, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { TFT_FRAMEWDT, 90, 2, WA_CENTER };
const WidgetConfig apPassConf     PROGMEM = { TFT_FRAMEWDT, 128, 2, WA_CENTER };
const WidgetConfig apPass2Conf    PROGMEM = { TFT_FRAMEWDT, 152, 2, WA_CENTER };
const WidgetConfig bootWdtConf    PROGMEM = { 0, 162, 1, WA_CENTER };
const WidgetConfig clockConf      PROGMEM = { TFT_FRAMEWDT, 104, 2, WA_RIGHT };
const WidgetConfig dateConf       PROGMEM = { TFT_FRAMEWDT, 158, 14, WA_LEFT };

// Speed, width, barwidth
const ProgressConfig bootPrgConf  PROGMEM = { 90, 14, 4 };

// left, top, width, height, textsize1, textsize2, align
const NamedayWidgetConfig namedayConf  PROGMEM = { TFT_FRAMEWDT, 132, 95, 36, 12, 14, WA_LEFT };

// left, top, width, height, segments, segWidth, segGap, segHeight, iconW, iconH, radius, border
const VolumeWidgetConfig volConf  PROGMEM = { 116, 213, 62, 20, 21, 1, 1, 6, 8, 10, 3, 1 };

// left, top, width, height, image1, image2, image3, image4
const WifiWidgetConfig wifiConf   PROGMEM = { 286, 213, 24, 20, "/images/wifi_1_22x18.png", "/images/wifi_2_22x18.png", "/images/wifi_3_22x18.png", "/images/wifi_4_22x18.png" };

// left, top, textsize, align, border, radius, fill, paddingX, paddingY, dimension
const BitrateBoxConfig bitrateConf PROGMEM = { TFT_FRAMEWDT, 104, 14, WA_CENTER, 1, 4, true, 0, 0, 41 };

/* VU METER  */
/* left, top, textsize, width, onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed, labelwidth, labelheight */
#define VU_HAS_DUAL_CONF 1
const VU_WidgetConfig vuConf              PROGMEM = { 29, 189, 8, 200, 5, 4, 2, 20, 8, 16, 6  };   // Standard: two VU rows
const VU_WidgetConfig vuConfBidirectional PROGMEM = { 17, 197, 8, 138, 5, 4, 2, 14, 6, 16, 6  };   // Boombox style: side by side

/* STRINGS  */
const char numtxtFmt[]  PROGMEM = "%d";
const char rssiFmt[]    PROGMEM = "WiFi %ddBm";
const char iptxtFmt[]   PROGMEM = "%s";
const char voltxtFmt[]  PROGMEM = "\023\025%d%%";
const char bitrateFmt[] PROGMEM = "%d kBs";

/* MOVES  */                              /* { left, top, width } */
const MoveConfig clockMove     PROGMEM = { 0, 120, -1 };

// clang-format on
