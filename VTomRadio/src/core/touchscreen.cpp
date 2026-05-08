#include "options.h"
#if (TS_MODEL != TS_MODEL_UNDEFINED) && (DSP_MODEL != DSP_DUMMY)
#    include "Arduino.h"
#    include "touchscreen.h"
#    include "config.h"
#    include "controls.h"
#    include "display.h"
#    include "player.h"
#    include "presets.h"
#    include "../displays/display_select.h" // DspCore + extern dsp
#    include "../pluginsManager/pluginsManager.h"
#    include "../plugins/backlight/backlight.h"

#    ifndef TS_STEPS
#        define TS_STEPS 40
#    endif

void TouchScreen::init(uint16_t w, uint16_t h) {
    // A LovyanGFX a dsp.init() során már inicializálta a touch IC-t a buszon.
    _width = w;
    _height = h;
}

tsDirection_e TouchScreen::_tsDirection(uint16_t x, uint16_t y) {
    int16_t dX = x - _oldTouchX;
    int16_t dY = y - _oldTouchY;
    // Az eredeti üzleti logika szerinti 20 pixeles küszöb
    if (abs(dX) > 20 || abs(dY) > 20) {
        if (abs(dX) > abs(dY)) {
            return (dX > 0) ? TSD_RIGHT : TSD_LEFT;
        } else {
            return (dY > 0) ? TSD_DOWN : TSD_UP;
        }
    }
    return TDS_REQUEST;
}

void TouchScreen::loop() {
    uint16_t             touchX = 0, touchY = 0;
    static uint32_t      touchLongPress;
    static tsDirection_e direct;
    static uint16_t      touchVol, touchStation;
#    if (DSP_WIDTH == 480) && (DSP_HEIGHT == 320)
    static uint32_t presetsLastActivity = 0;
    static int      presetActionDone = 0; // 0=play, 1=save, 2=del
    static int      presetHoldSlot = -1;
    static int      favHold = -1;
    static bool     favLongTriggered = false;
    static uint32_t lastTouchMs = 0;
#    endif
    static bool lastStTouched = false;

    if (!_checklpdelay(20, _touchdelay)) { return; }

    // ---- LovyanGFX touch beolvasás ----
    uint16_t x = 0, y = 0;
    bool     istouched = dsp.getTouch(&x, &y);

    // CSAK AKKOR frissítsük a touchX/Y-t, ha tényleg érintés van!
    // Ha istouched hamis, akkor a touchX/Y maradjon az előző érvényes érték.
    if (istouched) {
        if(config.store.dbgtouch) {
            Serial.printf("[TOUCH] raw x=%d y=%d  screen=%dx%d\n", x, y, _width, _height);
        }
        touchX = x;
        touchY = y;

        if (config.store.xTouchMirroring) {
            touchX = _width - 1 - touchX;
        }
        if (config.store.yTouchMirroring) {
            touchY = _height - 1 - touchY;
        }

        lastTouchMs = millis();
    }

    bool stTouched = (istouched || ((uint32_t)(millis() - lastTouchMs) < 120));
    bool newTouch = (stTouched && !lastStTouched);
    bool endTouch = (!stTouched && lastStTouched);

#    if (DSP_WIDTH == 480) && (DSP_HEIGHT == 320)
    // Auto-exit presets screen after 15s
    if (display.mode() == PRESETS) {
        if (presets_toastExpired()) { return; }
        if (presetsLastActivity == 0) { presetsLastActivity = millis(); }
        if (stTouched) {
            presetsLastActivity = millis();
        } else if ((uint32_t)(millis() - presetsLastActivity) > 15000UL) {
            display.putRequest(NEWMODE, PLAYER);
            presetsLastActivity = 0;
            direct = TSD_STAY;
            return;
        }
    } else {
        presetsLastActivity = 0;
    }
#    endif

    if (stTouched) {
        if (newTouch) { /************************** START TOUCH *************************************/
            _oldTouchX = touchX;
            _oldTouchY = touchY;
            touchVol = touchX;
            touchStation = touchY;
            direct = TDS_REQUEST;
            touchLongPress = millis();

#    if (DSP_WIDTH == 480) && (DSP_HEIGHT == 320)
            if (display.mode() == PRESETS && !presets_keyboardActive()) {
                presetHoldSlot = presets_hitTest(touchX, touchY);
                presetActionDone = 0;
                presets_setPressedSlot(presetHoldSlot);
                presets_drawPressed();
                favHold = presets_hitTestFav(touchX, touchY);
                favLongTriggered = false;
            }
#    endif
        } else { /************************** HOLD / SWIPE TOUCH *************************************/
            if (display.mode() == PRESETS) {
                direct = TDS_REQUEST;
            } else {
                // KRITIKUS: Csak akkor nézzük a mozgást, ha a hardver MÉG ÉRZÉKELI az ujjad (istouched)
                // Ha az ujjad már nincs a kijelzőn, ez az ág átugrásra kerül, hiába igaz még a stTouched!
                if (_oldTouchY >= 50 && istouched) {

                    int16_t distX = abs(touchX - _oldTouchX);
                    int16_t distY = abs(touchY - _oldTouchY);

                    // 60 pixeles holtsáv a biztonság kedvéért
                    if (distX < 30 && distY < 30) {
                        direct = TDS_REQUEST;
                    } else {
                        direct = _tsDirection(touchX, touchY);
                    }
                  //  Serial.printf("_oldTouchX: %d, _oldTouchY: %d\n", _oldTouchX, _oldTouchY); // Debug: érintés kezdete
                  //  Serial.printf("touchX: %d, touchY: %d\n", touchX, touchY);                 // Debug: aktuális érintési koordináták
                  //  Serial.printf("Touch hold: distX=%d, distY=%d\n", distX, distY);         // Debug: távolságok kiírása
                  //  Serial.printf("Determined direction: %d\n\n", direct);                         // Debug: meghatározott irány kiírása
                    switch (direct) {
                        case TSD_LEFT:
                        case TSD_RIGHT: {
                            // Ha valódi a swipe, eltoljuk az időt, hogy ne legyen Start/Stop a végén
                            touchLongPress = millis() - 5000;

                            if (display.mode() == PLAYER || display.mode() == VOL) {
                                int16_t xDelta = map(abs(touchVol - touchX), 0, _width, 0, TS_STEPS);
                                if (xDelta >= 1) {
                                    display.putRequest(NEWMODE, VOL);
                                    controlsEvent((touchVol - touchX) < 0);
                                    touchVol = touchX;
                                }
                            }
                            break;
                        }
                        case TSD_UP:
                        case TSD_DOWN: {
                            touchLongPress = millis() - 5000;
                            if (display.mode() == PLAYER || display.mode() == STATIONS) {
                                int16_t yDelta = map(abs(touchStation - touchY), 0, _height, 0, TS_STEPS);
                                if (yDelta >= 1) {
                                    display.putRequest(NEWMODE, STATIONS);
                                    controlsEvent((touchStation - touchY) < 0);
                                    touchStation = touchY;
                                }
                            }
                            break;
                        }
                        default: break;
                    }
                }
            }
        }
/********************************** PRESETS HOLD (real-time) ************************************/
#    if (DSP_WIDTH == 480) && (DSP_HEIGHT == 320)
        if (display.mode() == PRESETS && !presets_keyboardActive()) {
            uint32_t held = millis() - touchLongPress;

            // FAV átnevezés (hosszú nyomás)
            if (favHold >= 0 && !favLongTriggered) {
                if (held >= BTN_PRESS_TICKS * 2) {
                    presets_keyboardOpen((uint8_t)favHold);
                    presets_drawScreen();
                    favLongTriggered = true;
                    direct = TSD_STAY;
                }
            }

            // Mentés / Törlés logika (idő alapú)
            if (presetHoldSlot >= 0) {
                presets_drawHoldBar(held);
                presets_setPressedSlot(presetHoldSlot);
                presets_drawPressed();

                if (held >= BTN_PRESS_TICKS * 4)
                    presetActionDone = 2; // DEL (2mp)
                else if (held >= BTN_PRESS_TICKS * 2)
                    presetActionDone = 1; // SAVE (1mp)
                else
                    presetActionDone = 0; // PLAY
            }
        }
#    endif
    } else {
        if (endTouch) { /**************************** END TOUCH *********************************/
            if (direct == TDS_REQUEST) {
                uint32_t pressTicks = millis() - touchLongPress;

#    if (DSP_WIDTH == 480) && (DSP_HEIGHT == 320)
                presets_clearPressed();
                presets_setPressedSlot(-1);

                // 1) PRESETS HÍVÁS: Felső sáv (50px) érintése a főképernyőn
                if (_oldTouchY < 50 && pressTicks > 50 && pressTicks < BTN_PRESS_TICKS * 2) {
                    if (display.mode() == PLAYER) {
                        display.putRequest(NEWMODE, PRESETS);
                        presetsLastActivity = millis();
                    } else if (display.mode() == PRESETS) {
                        display.putRequest(NEWMODE, PLAYER);
                        presetsLastActivity = 0;
                    }
                    goto finish_touch;
                }

                // 2) PRESETS MÓD BELSŐ KEZELÉSE
                if (display.mode() == PRESETS) {
                    if (presets_keyboardActive()) {
                        if (pressTicks > 50) presets_keyboardTap(_oldTouchX, _oldTouchY);
                    } else {
                        // FAV Bank választás
                        int fav = presets_hitTestFav(_oldTouchX, _oldTouchY);
                        if (fav >= 0) {
                            if (!favLongTriggered && pressTicks > 50) presets_selectBank((uint8_t)fav);
                            presets_drawScreen();
                        } else {
                            // Preset slot kezelés (Play/Save/Del a hold fázis alapján)
                            int hit = presets_hitTest(_oldTouchX, _oldTouchY);
                            if (hit >= 0) {
                                if (presetActionDone == 2)
                                    presets_clear((uint8_t)hit);
                                else if (presetActionDone == 1)
                                    presets_save((uint8_t)hit);
                                else {
                                    if (!presets_play((uint8_t)hit)) display.putRequest(NEWMODE, PLAYER);
                                }
                            }
                        }
                    }
                    goto finish_touch;
                }
#    endif

                // 3) STOP/START FUNKCIÓ: Főképernyő (kivéve felső 50px), rövid érintés
                if (pressTicks > 50 && pressTicks < BTN_PRESS_TICKS * 2) {
                    bool consumedByPlugin = false;
                    // Kompatibilitás: a backlight ébresztés maradjon elsődleges, mint a régi működésben.
#    if (BRIGHTNESS_PIN != 255)
                    if (config.store.fadeEnabled && backlightPlugin.isFadeControl()) {
                        consumedByPlugin = true;
                    }
#    endif
                    if (!consumedByPlugin) {
                        pm.on_user_activity(consumedByPlugin);
                    }
                    if (consumedByPlugin) {
                        // Csak a fény jön vissza, a zene nem áll meg
                    } else {
                        onBtnClick(EVT_BTNCENTER); // STOP / START toggle
                    }
                }
            }

        finish_touch:
#    if (DSP_WIDTH == 480) && (DSP_HEIGHT == 320)
            presetHoldSlot = -1;
            presetActionDone = 0;
#    endif
            direct = TSD_STAY;
        }
    }
    lastStTouched = stTouched;
}

bool TouchScreen::_checklpdelay(int m, uint32_t& tstamp) {
    if (millis() - tstamp > (uint32_t)m) {
        tstamp = millis();
        return true;
    }
    return false;
}

#endif