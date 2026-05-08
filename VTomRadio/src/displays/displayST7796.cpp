#include "../core/options.h"
#if DSP_MODEL == DSP_ST7796
#    include "display_select.h"
#    include "../core/config.h"

DspCore::DspCore() {}
void DspCore::initDisplay() {
    Serial.printf("displayST7796.cpp: initDisplay()\n");

#if TS_MODEL == TS_MODEL_XPT2046
    // Kényszerítsük a Touch-ot, hogy engedje el az SPI buszt (XPT2046 only)
    if (TS_CS != 255) {
        pinMode(TS_CS, OUTPUT);
        digitalWrite(TS_CS, HIGH);
    }
#endif

    // Csak ezutan johet a Lovyan init
    if (!init()) {
        Serial.println("LGFX Init FAIL");
    } else {
        Serial.println("LGFX Init OK");
    }
#if TS_MODEL == TS_MODEL_FT6X36
    if (touch()) {
        Serial.println("Touch driver: found");
    } else {
        Serial.println("Touch driver: NOT found / init failed");
    }
#endif
    setTextWrap(false);
    setTextSize(1);
    fillScreen(0x0000);
    invert();
    flip();
    Serial.printf("displayST7796.cpp: initDisplay() DONE\n");
}

void DspCore::clearDsp(bool black) {
    fillScreen(black ? 0 : config.theme.background);
}

void DspCore::flip() {
    uint8_t rotation = static_cast<uint8_t>(DEFAULT_SCREEN_ROTATION & 0x03);
    if (config.store.flipscreen) {
        rotation = static_cast<uint8_t>((rotation + 2) & 0x03);
    }
    setRotation(rotation);
}

void DspCore::invert() {
    invertDisplay(config.store.invertdisplay);
}

void DspCore::sleep(void) {
    writeCommand(ST7796_SLPIN);
    delay(150);
    writeCommand(ST7796_DISPOFF);
    delay(150);
}

void DspCore::wake(void) {
    writeCommand(ST7796_DISPON);
    delay(150);
    writeCommand(ST7796_SLPOUT);
    delay(150);
}

void DspCore::createBootSprite() {
    if (_bootSpr) return;

    _bootSpr = new LGFX_Sprite(this);
    _bootSpr->setPsram(true);
    _bootSpr->createSprite(width(), height());
}

void DspCore::deleteBootSprite() {
    if (_bootSpr) {
        _bootSpr->deleteSprite();
        delete _bootSpr;
        _bootSpr = nullptr;
    }
}

LGFX_Sprite* DspCore::getBootSprite() {
    return _bootSpr;
}

#endif
