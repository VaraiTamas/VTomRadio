#pragma once

#include "../core/options.h"

#if DSP_MODEL==DSP_DUMMY
  #define DUMMYDISPLAY
  #define DSP_NOT_FLIPPED

#elif DSP_MODEL==DSP_ST7789
  #define TIME_SIZE           52
  #include "displayST7789.h"

#elif DSP_MODEL==DSP_ILI9341
  #define TIME_SIZE           52
  #include "displayILI9341.h"

#elif DSP_MODEL==DSP_ST7796
  #define TIME_SIZE           70
  #include "displayST7796.h"

#elif DSP_MODEL==DSP_ILI9488 || DSP_MODEL==DSP_ILI9486
   #include "displayILI9488.h"
#elif DSP_MODEL==DSP_SSD1322        
  #define TIME_SIZE           35
  #define DSP_OLED
  #include "displaySSD1322.h"
#endif

