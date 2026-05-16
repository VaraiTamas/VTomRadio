// clang-format off
#pragma once

#include "../../core/config.h"
#include "../../core/display.h"
#include "../widgets/widgets.h"

#define DSP_WIDTH       480
#define DSP_HEIGHT      320
#define TFT_FRAMEWDT     10
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define CLOCK_HEIGHT    100

#if BITRATE_FULL
  #define TITLE_FIX  44
#else
  #define TITLE_FIX   0
#endif
#define bootLogoTop 55

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf       PROGMEM = {{ TFT_FRAMEWDT, 0, 36, WA_CENTER }, 140, true, MAX_WIDTH, 5000, 3, 40 };
const ScrollConfig title1Conf     PROGMEM = {{TFT_FRAMEWDT, 55, 20, WA_CENTER}, 140, true, MAX_WIDTH, 5000, 4, 40};
const ScrollConfig title2Conf     PROGMEM = {{TFT_FRAMEWDT, 81, 20, WA_CENTER}, 140, true, MAX_WIDTH, 5000, 4, 40};
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 146, 24, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 7, 40 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_CENTER }, 140, false, MAX_WIDTH, 0, 7, 40 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, 320-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 5, 40 };
const ScrollConfig weatherConf    PROGMEM = {{ TFT_FRAMEWDT, 107, 20, WA_CENTER }, 140, false, MAX_WIDTH, 5000, 4, 40 };

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{TFT_FRAMEWDT, 45, 0, WA_CENTER}, DSP_WIDTH - TFT_FRAMEWDT*2, 1, true}; // Csík rajzolása a rádióadó neve alá.
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false };
const FillConfig   volbarConf     PROGMEM = {{TFT_FRAMEWDT, DSP_HEIGHT - TFT_FRAMEWDT - 8, 0, WA_LEFT}, MAX_WIDTH, 5, true};
const FillConfig   playlBGConf    PROGMEM = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false };
const FillConfig   heapbarConf    PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH-20, 2, false };

// left,top, width, height, textsize, align, border, radius, fill, paddingX, paddingY
const textBoxConfig bootstrConf   PROGMEM = {60, 255, 360, 35, 20, WA_CENTER, 1, 4, true, 0, 0};  // Boot szöveg a boot oldalra, középre igazítva, kerettel és háttérrel.
const textBoxConfig ipBoxConf     PROGMEM = {TFT_FRAMEWDT, 288, 150, 25, 18, WA_CENTER, 1, 4, true, 0, 0};
const textBoxConfig chBoxConf     PROGMEM = {290, 288, 80, 25, 18, WA_CENTER, 1, 4, true, 0, 0};
const textBoxConfig rssiBoxConf   PROGMEM = {390, 288, 80, 25, 18, WA_CENTER, 1, 4, true, 0, 0};

/* WIDGETS  */ /* { left, top, fontsize, align } */
const WidgetConfig numConf        PROGMEM = {0, 150, 70, WA_CENTER};  // hangerő állítás
const WidgetConfig apNameConf     PROGMEM = {TFT_FRAMEWDT, 88, 3, WA_CENTER};  // AP mode
const WidgetConfig apName2Conf    PROGMEM = {TFT_FRAMEWDT, 120, 3, WA_CENTER}; // AP mode
const WidgetConfig apPassConf     PROGMEM = {TFT_FRAMEWDT, 173, 3, WA_CENTER}; // AP mode
const WidgetConfig apPass2Conf    PROGMEM = {TFT_FRAMEWDT, 205, 3, WA_CENTER}; // AP mode
const WidgetConfig bootWdtConf    PROGMEM = {0, 220, 1, WA_CENTER};  // Boot progress bar, középre igazítva, "align" nincs használva, mert a ProgressWidget nem használja ezt a paramétert.
const WidgetConfig clockConf      PROGMEM = {10, 145, 2, WA_RIGHT,};
const WidgetConfig dateConf       PROGMEM = { TFT_FRAMEWDT, 220, 1, WA_LEFT }; 

// Speed, width, barwidth
const ProgressConfig bootPrgConf  PROGMEM = {90, 14, 4};

//left, top, width, height, textsize1, textsize2, align
const NamedayWidgetConfig namedayConf    PROGMEM = { TFT_FRAMEWDT, 180, 140, 45, 18, 20, WA_LEFT};  // Módosítás új sor "nameday"

// left, top, width, height, segments, segWidth, segGap, segHeight, iconW, iconH, radius, border
const VolumeWidgetConfig volConf PROGMEM = {180, 288, 90, 25, 21, 2, 1, 8, 10, 14, 4, 1};

// left, top, width, height, image1, image2, image3, image4
const WifiWidgetConfig wifiConf PROGMEM = {440, 288, 30, 25, "/images/wifi_1_30x25.png", "/images/wifi_2_30x25.png", "/images/wifi_3_30x25.png", "/images/wifi_4_30x25.png"};

//left, top, textsize, align, border, radius, fill, paddingX, paddingY, dimension
const BitrateBoxConfig bitrateConf PROGMEM = {10, 145, 20, WA_CENTER, 1, 4, true, 0, 0, 56};

/* left, top, textsize, width, onebandwidth (width), onebandheight (height), bandsHspace (space), bandsVspace (vspace), numofbands (perheight), fadespeed, labelwidth, labelheight} */
#define VU_HAS_DUAL_CONF 1
const VU_WidgetConfig vuConf PROGMEM = {37, 258, 9, 300, 7, 6, 2, 30, 11, 22, 12};              // ALAP VU: két VU egymás alatt
const VU_WidgetConfig vuConfBidirectional PROGMEM = {35, 265, 9, 200, 7, 6, 2, 20, 9, 22, 12}; // BOOMBOX_STYLE: két VU egymás mellett

/* STRINGS  */
const char numtxtFmt[]  PROGMEM = "%d";
const char rssiFmt[]    PROGMEM = "WiFi %ddBm";
const char iptxtFmt[]   PROGMEM = "%s";
const char voltxtFmt[]  PROGMEM = "\023\025%d%%"; //Original "\023\025%d" Módosítás "vol_step"  
const char bitrateFmt[] PROGMEM = "%d kBs";

/* MOVES  */ /* { left, top, width } */
const MoveConfig clockMove     PROGMEM = {0, 176, -1}; // A clock widget pozíciója, a width -1 azt jelenti, hogy a moveTo() függvényben a moveBack() logikát használjuk, vagyis visszaállítjuk az eredeti pozícióra.

// clang-format on