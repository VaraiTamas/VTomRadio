/*
TTF font to VLW konvertálás: https://vlw-font-creator.m5stack.com/
- Basic Latin karakterek: 0x20-0x7E
- Latin-1 Suplement karakterek: 0xA0-0xFF // Ez a tartomány nem minden fontnál van meg, de ha van, akkor érdemes betölteni, mert vannak benne hasznos karakterek, pl. a ° jel.
- Latin Extended-A karakterek: 0x100-0x17F // Ez a tartomány sem minden fontnál van meg, de ha van, akkor érdemes betölteni, mert vannak benne hasznos karakterek, pl. a ° jel.
- Grek and Coptic karakterek: 0x370-0x3FF 
- Cyrillic karakterek: 0x400-0x4FF 
- Cyrillic Supplement karakterek: 0x500-0x52F
- Clock fontoknál 0x30-0x39 (0-9) és 0x3A (:) karakterek vannak benne, mert ezekre van szükség a számlap megjelenítéséhez.

- Nyelvi karakterkészletek: 0x20-0x7E, 0xA0-0xFF, 0x100-0x17F, 0x370-0x3FF, 0x400-0x4FF, 0x500-0x52F
- Az óra karakterkészlete: 0x30-0x39, 0x3A

*/

#pragma once

#include <stdint.h>
#include <LovyanGFX.hpp>

// ================= VLW FONTOK =================
extern uint8_t* font_vlw_9;
extern uint8_t* font_vlw_12;
extern uint8_t* font_vlw_16;
extern uint8_t* font_vlw_18;
extern uint8_t* font_vlw_20;
extern uint8_t* font_vlw_22;
extern uint8_t* font_vlw_24;
extern uint8_t* font_vlw_26;
extern uint8_t* font_vlw_36;
extern uint8_t* font_vlw_clock;
extern uint8_t* font_vlw_clock_sec;

// ================= GFX FONT POINTEREK =================
using GFXfont = lgfx::v1::GFXfont;
using GFXglyph = lgfx::v1::GFXglyph;

// ================= BETÖLTÉS =================
bool loadFonts();
void freeFonts();
void setClockFontStyle(uint8_t style);
void getClockFontStylePointers(uint8_t style, uint8_t** mainFont, uint8_t** secFont);

// ================= MÉRET SEGÉDEK =================
uint8_t  font_charWidth(unsigned char c, bool sec = false);
uint16_t font_textHeight(bool sec = false);
uint16_t font_textWidth(const char* txt, bool sec = false);