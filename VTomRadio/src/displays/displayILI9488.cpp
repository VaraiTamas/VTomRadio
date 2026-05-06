#include "../core/options.h"
#if DSP_MODEL == DSP_ILI9488 || DSP_MODEL == DSP_ILI9486
#    include "display_select.h"
#    include "../core/config.h"

DspCore::DspCore() {}
void DspCore::initDisplay() {
    Serial.printf("displayILI9488.cpp: initDisplay()\n");

    // Kényszerítsük a Touch-ot, hogy engedje el az SPI buszt
    pinMode(TS_CS, OUTPUT);
    digitalWrite(TS_CS, HIGH);

    // Csak ezután jöhet a Lovyan init
    if (!init()) {
        Serial.println("LGFX Init FAIL");
    } else {
        Serial.println("LGFX Init OK");
    }
    // setRotation(1);
    setTextWrap(false);
    setTextSize(1);
    fillScreen(0x0000);
    invert();
    flip();
    Serial.printf("displayILI9488.cpp: initDisplay() DONE\n");
}

void DspCore::clearDsp(bool black) {
    fillScreen(black ? 0 : config.theme.background);
}

void DspCore::flip() {
    setRotation(config.store.flipscreen ? 3 : 1);
}

void DspCore::invert() {
    invertDisplay(config.store.invertdisplay);
}

void DspCore::sleep(void) {
    writeCommand(ILI9488_SLPIN);
    delay(150);
    writeCommand(ILI9488_DISPOFF);
    delay(150);
}

void DspCore::wake(void) {
    writeCommand(ILI9488_DISPON);
    delay(150);
    writeCommand(ILI9488_SLPOUT);
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