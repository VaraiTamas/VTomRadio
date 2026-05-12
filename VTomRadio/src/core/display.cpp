#include "Arduino.h"
#include "options.h"
#include "WiFi.h"
#include "time.h"
#include "config.h"
#include "display.h"
#include "presets.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"
#include "../displays/display_select.h"
#include "../displays/widgets/widgets.h"
#include "../displays/widgets/pages.h"
#include "../displays/tools/language.h"
#include "fonts.h"
#include "serial_littlefs.h"
#include "touchscreen.h"
// #define LGFX_USE_PNG

Display display;
#ifdef USE_NEXTION
#    include "../displays/nextion.h"
Nextion nextion;
#endif

#ifndef CORE_STACK_SIZE
#    define CORE_STACK_SIZE 1024 * 8 // 4
#endif
#ifndef DSP_TASK_PRIORITY
#    define DSP_TASK_PRIORITY 3 //"task_prioritas"
#endif
#ifndef DSP_TASK_CORE_ID
#    define DSP_TASK_CORE_ID 0
#endif
#ifndef DSP_TASK_DELAY
#    define DSP_TASK_DELAY pdMS_TO_TICKS(30) // cap for 50 fps
#endif

#define DSP_QUEUE_TICKS 0

#ifndef DSQ_SEND_DELAY
#    define DSQ_SEND_DELAY pdMS_TO_TICKS(200)
#endif

QueueHandle_t displayQueue;

static void loopDspTask(void* pvParameters) {
    while (true) {
        // Serial.printf("STACK FREE: %u\n", uxTaskGetStackHighWaterMark(NULL));
#ifndef DUMMYDISPLAY
        if (displayQueue == NULL) { break; }
        if (timekeeper.loop0()) {
#    if (TS_MODEL != TS_MODEL_UNDEFINED) && (DSP_MODEL != DSP_DUMMY)
            if (network.status == CONNECTED || network.status == SDREADY) touchscreen.loop();
#    endif
#    ifdef NAMEDAYS_FILE
            display.loopNameday(false);
#    endif
            display.loop();
#    ifndef NETSERVER_LOOP1
            netserver.loop();
#    endif
        }
#else
        timekeeper.loop0();
#    ifndef NETSERVER_LOOP1
        netserver.loop();
#    endif
#endif
        vTaskDelay(DSP_TASK_DELAY);
    }
    vTaskDelete(NULL);
}

void Display::_createDspTask() {
    xTaskCreatePinnedToCore(loopDspTask, "DspTask", CORE_STACK_SIZE, NULL, DSP_TASK_PRIORITY, NULL, DSP_TASK_CORE_ID); //"task_prioritas"
}

#ifndef DUMMYDISPLAY
//============================================================================================================================
DspCore dsp;

Page* pages[] = {new Page(), new Page(), new Page(), new Page(), new Page()}; // "presets" +1 page

#    if !((DSP_MODEL == DSP_ST7735 && DTYPE == INITR_BLACKTAB) || DSP_MODEL == DSP_ST7789 || DSP_MODEL == DSP_ST7796 || DSP_MODEL == DSP_ILI9488 || DSP_MODEL == DSP_ILI9486 || \
          DSP_MODEL == DSP_ILI9341 || DSP_MODEL == DSP_ILI9225 || DSP_MODEL == DSP_ST7789_170 || DSP_MODEL == DSP_SSD1322)
#        undef BITRATE_FULL
#        define BITRATE_FULL false
#    endif

void returnPlayer() {
    display.putRequest(NEWMODE, PLAYER);
}

Display::~Display() {
    delete _pager;
    delete _footer;
    delete _plwidget;
    delete _nums;
    delete _clock;
    delete _meta;
    delete _title1;
    delete _title2;
    delete _plcurrent;
}

void Display::init() {
    Serial.print("##[BOOT]#\tdisplay.init\t\n");
#    ifdef USE_NEXTION
    nextion.begin();
#    endif
#    if LIGHT_SENSOR != 255
    analogSetAttenuation(ADC_0db);
#    endif
    _bootStep = 0;
    // --- HARDVER INIT ---
    dsp.initDisplay(); // void DspCore::initDisplay() - Ez a függvény hívja meg a display inicializálását, ami a DspCore osztályban van definiálva. Ez a függvény felelős a kijelző beállításáért és
                       // előkészítéséért a használatra.



    if (!loadFonts()) {
        Serial.println("[FONT] HIBA: Egy vagy tobb kotelezo font hianyzik a LittleFS-rol!");
    } else {
        Serial.println("[FONT] Kotelezo fontok sikeresen betoltve.");
    }

    // --- QUEUE ---
    displayQueue = xQueueCreate(5, sizeof(requestParams_t));
    while (displayQueue == NULL) { delay(1); }
    _pager = new Pager();
    _footer = new Page();
    _plwidget = new PlayListWidget();
    _nums = new NumWidget();
    _clock = new ClockWidget();
    _meta = new ScrollWidget();
    _title1 = new ScrollWidget();
    _plcurrent = new ScrollWidget();
    _createDspTask();
    while (_bootStep == 0) { delay(10); }
    Serial.println("done");
}

uint16_t Display::width() {
    return dsp.width();
}

uint16_t Display::height() {
    return dsp.height();
}

void Display::_bootScreen() {
    if (!_pager) return;
    _boot = new Page();
    _boot->addWidget(new ProgressWidget(bootWdtConf, bootPrgConf, 0xFFFF, 0));
    _bootstring = (textBoxWidget*)&_boot->addWidget(
        new textBoxWidget(bootstrConf, 50, true, 0xFFFF, 0, config.theme.div)); // Módosítás: textBoxWidget használata a boot szöveghez, hogy a hosszabb szövegek is elférjenek.
    _pager->addPage(_boot);
    _pager->setPage(_boot, true);
    dsp.drawLogo(bootLogoTop);
    _bootStep = 1;
}

void Display::_buildPager() {
    _meta->init("*", metaConf, config.theme.meta, config.theme.metabg);
    _title1->init("*", title1Conf, config.theme.title1, config.theme.background);
    _clock->init(clockConf, 0, 0);
    _plcurrent->init("*", playlistConf, config.theme.plcurrent, config.theme.plcurrentbg); // scrollwidget
    _plwidget->init(_plcurrent);
    _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf.width});

#    ifndef HIDE_TITLE2
    _title2 = new ScrollWidget("*", title2Conf, config.theme.title2, config.theme.background);
#    endif

    _plbackground = new FillWidget(playlBGConf, config.theme.plcurrentfill);

#    if DSP_INVERT_TITLE || defined(DSP_OLED)
    _metabackground = new FillWidget(metaBGConf, config.theme.metafill);
#    else
    _metabackground = new FillWidget(metaBGConfInv, config.theme.metafill);
#    endif

#    ifndef HIDE_VU
#        ifdef VU_HAS_DUAL_CONF
    _vuwidget = new VuWidget(vuConf, vuConfBidirectional, config.theme.vumax, config.theme.vumid, config.theme.vumin, config.theme.background);
#        else
    _vuwidget = new VuWidget(vuConf, config.theme.vumax, config.theme.vumid, config.theme.vumin, config.theme.background);
#        endif
#    endif

#    ifndef HIDE_HEAPBAR
    _heapbar = new SliderWidget(heapbarConf, config.theme.buffer, config.theme.background, psramInit() ? 300000 : 1600 * config.store.abuff);
#    endif

#    ifndef HIDE_VOL
    _volwidget = new VolumeWidget(&dsp, volConf);
#    endif

#    ifndef HIDE_IP
    _ipbox = new textBoxWidget(ipBoxConf, 30, false, config.theme.ip, config.theme.ip_bg, config.theme.ip_border);
#    endif

#    ifndef HIDE_RSSI
    _wifiwidget = new WifiWidget(&dsp, &wifiConf);
    _rssibox = new textBoxWidget(rssiBoxConf, 16, false, config.theme.rssi, config.theme.rssi_bg, config.theme.rssi_border);
    if (config.store.rssiAsText) {
        _wifiwidget->lock();
    } else {
        _rssibox->lock();
    }
#    endif

    _nums->init(numConf, 10, false, config.theme.digit, config.theme.background);

#    ifndef HIDE_WEATHER
    _weather = new ScrollWidget("\007", weatherConf, config.theme.weather, config.theme.background);
#    endif

    _chbox = new textBoxWidget(chBoxConf, 12, false, config.theme.ch, config.theme.ch_bg, config.theme.ch_border);
    if (_chbox) { _footer->addWidget(_chbox); }
    if (_volwidget) { _footer->addWidget(_volwidget); }
    if (_ipbox) { _footer->addWidget(_ipbox); }
    if (_wifiwidget) { _footer->addWidget(_wifiwidget); }
    if (_rssibox) { _footer->addWidget(_rssibox); }
    if (_heapbar) { _footer->addWidget(_heapbar); }
    if (_metabackground) { pages[PG_PLAYER]->addWidget(_metabackground); }
    pages[PG_PLAYER]->addWidget(_meta);
    pages[PG_PLAYER]->addWidget(_title1);
    if (_title2) { pages[PG_PLAYER]->addWidget(_title2); }
    if (_weather) { pages[PG_PLAYER]->addWidget(_weather); }

    _bitratewidget = new BitrateWidget(bitrateConf, config.theme.bitrate, config.theme.background);
    _bitratewidget->setActive(true);
    pages[PG_PLAYER]->addWidget(_bitratewidget);

#    ifdef NAMEDAYS_FILE
    _namedaywidget = new NamedayWidget();
    _namedaywidget->init(namedayConf, config.theme.nameday, config.theme.background);
    _namedaywidget->setActive(true);
    pages[PG_PLAYER]->addWidget(_namedaywidget);
#    endif

    if (_vuwidget) { pages[PG_PLAYER]->addWidget(_vuwidget); }
    pages[PG_PLAYER]->addWidget(_clock);
    pages[PG_SCREENSAVER]->addWidget(_clock);
    pages[PG_PLAYER]->addPage(_footer);
    if (_metabackground) { pages[PG_DIALOG]->addWidget(_metabackground); }
    pages[PG_DIALOG]->addWidget(_meta);
    pages[PG_DIALOG]->addWidget(_nums);
    pages[PG_DIALOG]->addPage(_footer);
    pages[PG_PLAYLIST]->addWidget(_plcurrent); // scrollwidget
    pages[PG_PLAYLIST]->addWidget(_plwidget);
    for (const auto& p : pages) { _pager->addPage(p); }
}

void Display::_apScreen() {
    if (_boot) { _pager->removePage(_boot); }
    _boot = new Page();
#    if DSP_INVERT_TITLE || defined(DSP_OLED)
    _boot->addWidget(new FillWidget(metaBGConf, config.theme.metafill));
#    else
    _boot->addWidget(new FillWidget(metaBGConfInv, config.theme.metafill));
#    endif
    ScrollWidget* bootTitle = (ScrollWidget*)&_boot->addWidget(new ScrollWidget("*", apTitleConf, config.theme.meta, config.theme.metabg));
    bootTitle->setText("VTom Radio AP Mode");
    TextWidget* apname = (TextWidget*)&_boot->addWidget(new TextWidget(apNameConf, 30, false, config.theme.title1, config.theme.background));
    apname->setText(LANG::apNameTxt);
    TextWidget* apname2 = (TextWidget*)&_boot->addWidget(new TextWidget(apName2Conf, 30, false, config.theme.clock, config.theme.background));
    apname2->setText(apSsid);
    TextWidget* appass = (TextWidget*)&_boot->addWidget(new TextWidget(apPassConf, 30, false, config.theme.title1, config.theme.background));
    appass->setText(LANG::apPassTxt);
    TextWidget* appass2 = (TextWidget*)&_boot->addWidget(new TextWidget(apPass2Conf, 30, false, config.theme.clock, config.theme.background));
    appass2->setText(apPassword);
    ScrollWidget* bootSett = (ScrollWidget*)&_boot->addWidget(new ScrollWidget("*", apSettConf, config.theme.title2, config.theme.background));
    bootSett->setText(config.ipToStr(WiFi.softAPIP()), LANG::apSettFmt);
    _pager->addPage(_boot);
    _pager->setPage(_boot);
}

void Display::_start() {
    if (_boot) { _pager->removePage(_boot); }
#    ifdef USE_NEXTION
    nextion.wake();
#    endif
    if (network.status != CONNECTED && network.status != SDREADY) {
        _apScreen();
#    ifdef USE_NEXTION
        nextion.apScreen();
#    endif
        _bootStep = 2;
        return;
    }
#    ifdef USE_NEXTION
    // nextion.putcmd("page player");
    nextion.start();
#    endif
    _buildPager();
    _mode = PLAYER;
    config.setTitle(LANG::const_PlReady);

    _pager->setPage(pages[PG_PLAYER]);

    if (_heapbar) {
        _heapbar->lock(!config.store.audioinfo);
        if (config.store.audioinfo) { _heapbar->setValue(player.inBufferFilled()); }
    }

    if (_weather) { _weather->lock(!config.store.showweather); }
    if (_weather && config.store.showweather) { _weather->setText(LANG::const_getWeather); } // Üres string.

    if (_vuwidget) { _vuwidget->lock(); }

    if (_wifiwidget) { _wifiwidget->setRSSI(WiFi.RSSI()); }
#    ifndef HIDE_RSSI
    _applyRssiMode();
#    endif
    if (_chbox) {
        _chbox->setText(config.lastStation(), "Ch:%d."); // Beállítja a csatorna számát a widgetnek.
    }
#    ifndef HIDE_IP
    if (_ipbox) { _ipbox->setText(config.ipToStr(WiFi.localIP()), iptxtFmt); }
#    endif
    _volume();
    _station();
    _time(false);
    _bootStep = 2;
    pm.on_display_player();
} // _start vége

void Display::_showDialog(const char* title) {
    dsp.setScrollId(NULL);
    _pager->setPage(pages[PG_DIALOG]);
#    ifdef META_MOVE
    _meta->moveTo(metaMove);
#    endif
    _meta->setAlign(WA_CENTER);
    _meta->setText(title);
} // _showDialog vége

void Display::_refreshThemeColors() {
    if (_meta) { _meta->setColors(config.theme.meta, config.theme.metabg); }
    if (_title1) { _title1->setColors(config.theme.title1, config.theme.background); }
    if (_title2) { _title2->setColors(config.theme.title2, config.theme.background); }
    if (_metabackground) { _metabackground->setColors(config.theme.metafill, config.theme.metafill); }
    if (_weather) { _weather->setColors(config.theme.weather, config.theme.background); }
    if (_nums) { _nums->setColors(config.theme.digit, config.theme.background); }
    if (_plcurrent) { _plcurrent->setColors(config.theme.plcurrent, config.theme.plcurrentbg); }
    if (_plbackground) { _plbackground->setColors(config.theme.plcurrentfill, config.theme.plcurrentfill); }
    if (_ipbox) { _ipbox->setColors(config.theme.ip, config.theme.ip_bg, config.theme.ip_border); }
    if (_chbox) { _chbox->setColors(config.theme.ch, config.theme.ch_bg, config.theme.ch_border); }
    if (_rssibox) { _rssibox->setColors(config.theme.rssi, config.theme.rssi_bg, config.theme.rssi_border); }
    if (_heapbar) { _heapbar->setColors(config.theme.buffer, config.theme.background); }
}

void Display::_applyRssiMode() {
#    ifndef HIDE_RSSI
    const int currentRssi = WiFi.RSSI();

    if (config.store.rssiAsText) {
        if (_wifiwidget) { _wifiwidget->lock(true); }
        if (_rssibox) {
            _rssibox->lock(false);
            char buf[16];
            snprintf(buf, sizeof(buf), "%d dBm", currentRssi);
            _rssibox->setText(buf);
        }
    } else {
        if (_rssibox) { _rssibox->lock(true); }
        if (_wifiwidget) {
            _wifiwidget->setRSSI(currentRssi);
            _wifiwidget->lock(false);
        }
    }
#    endif
}

void Display::_swichMode(displayMode_e newmode) {
    Serial.printf("Display::_swichMode: %d\n", newmode);
#    ifdef USE_NEXTION
    // nextion.swichMode(newmode);
    nextion.putRequest({NEWMODE, newmode});
#    endif
    if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY)) { return; }
    _mode = newmode;
    dsp.setScrollId(NULL);
    if (newmode == PLAYER) {
        _clock->moveBack(); // Ha az óra nem az eredeti helyén van (pl. képernyővédő után), visszaállítjuk.
        _refreshThemeColors();
        numOfNextStation = 0;
        _meta->setAlign(metaConf.widget.align);
        _station();
        config.isScreensaver = false;
        _pager->setPage(pages[PG_PLAYER]);
        config.setDspOn(config.store.dspon, false);
        pm.on_display_player();
    }

    if (newmode == SCREENSAVER || newmode == SCREENBLANK) {
        config.isScreensaver = true;
        _pager->setPage(pages[PG_SCREENSAVER]);
        if (newmode == SCREENBLANK) {
            _clock->clear();
            config.setDspOn(false, false);
        }
    } else {
        config.screensaverTicks = SCREENSAVERSTARTUPDELAY;
        config.screensaverPlayingTicks = SCREENSAVERSTARTUPDELAY;
        config.isScreensaver = false;
#    if PWR_AMP != 255 // "PWR_AMP"
        digitalWrite(PWR_AMP, HIGH);
#    endif
    }

    if (newmode == VOL) {
#    ifndef HIDE_IP
        _showDialog(LANG::const_DlgVolume);
#    else
        _showDialog(config.ipToStr(WiFi.localIP()));
#    endif
        _nums->setText(config.store.volume, numtxtFmt);
    }
    if (newmode == LOST) { _showDialog(LANG::const_DlgLost); }
    if (newmode == UPDATING) { _showDialog(LANG::const_DlgUpdate); }
    if (newmode == SLEEPING) {
        _showDialog("SLEEPING");
        delay(2000);
        dsp.clearDsp();
        config.doSleepW();
    }
    if (newmode == SDCHANGE) { _showDialog(LANG::const_waitForSD); }
    if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) { _showDialog(LANG::const_DlgNextion); }
    if (newmode == NUMBERS) { _showDialog(""); }
#    if (DSP_MODEL == DSP_ILI9488) || (DSP_MODEL == DSP_ST7796)
    if (newmode == PRESETS) {
        _pager->setPage(pages[PG_PRESETS], true);
        presets_drawScreen();
    }
#    endif
    if (newmode == STATIONS) {
        _refreshThemeColors();
        // Drop stale player-mode draw requests so they cannot paint one extra old frame.
        resetQueue();
        // Prevent ScrollWidget pre-draw during page activation to avoid an empty-row flash.
        _plcurrent->lock(true);
        // Ensure no stale current-row text is visible before the fresh playlist draw.
        _plcurrent->setText("");
        _pager->setPage(pages[PG_PLAYLIST], true);
        currentPlItem = config.lastStation();
        // Átadjuk a scrollwidgetet, ha eddig nem tettük meg
        _plwidget->init(_plcurrent);
        _plcurrent->unlock();
        _drawPlaylist();
    }
} // _swichMode vége

void Display::resetQueue() {
    if (displayQueue != NULL) { xQueueReset(displayQueue); }
}

void Display::_drawPlaylist() {
    _plwidget->drawPlaylist(currentPlItem);
    timekeeper.waitAndReturnPlayer(config.store.stationsListReturnTime);
}

void Display::_drawNextStationNum(uint16_t num) {
    timekeeper.waitAndReturnPlayer(config.store.stationsListReturnTime); // Visszatérési idő a főképernyőre.
    _meta->setText(config.stationByNum(num));
    _nums->setText(num, "%d");
}

void Display::putRequest(displayRequestType_e type, int payload) {
    if (displayQueue == NULL) { return; }
    requestParams_t request;
    request.type = type;
    request.payload = payload;
    xQueueSend(displayQueue, &request, DSQ_SEND_DELAY);
#    ifdef USE_NEXTION
    nextion.putRequest(request);
#    endif
}

void Display::_layoutChange(bool played) { // Player START vagy STOP után hívódik meg, hogy a VU meter helyét és láthatóságát a config alapján beállítsa.
    if (config.store.vumeter && _vuwidget) {
        if (played) {
            if (_vuwidget) { _vuwidget->unlock(); }

        } else {
            if (_vuwidget) {
                if (!_vuwidget->locked()) { _vuwidget->lock(); }
            }
        }
    } else {
        if (played) {}
    }
}

void Display::applyVuModeChange() {
    if (!_vuwidget) { return; }
    _vuwidget->switchMode(config.store.vuBidirectional);
    _layoutChange(player.isRunning());
}

void Display::invalidateThemeWidgets() {
    if (!_wifiwidget || _locked) { return; }
    _wifiwidget->invalidate();
}

void Display::loop() {
    if (_bootStep == 0) {
        _pager->begin();
        _bootScreen();
        return;
    }
    if (_mode == STATIONS) {
        _plcurrent->loop(); // Ez hajtja az X irányú görgetést a lejátszási listában, ahol a hosszabb szövegek vannak.
    }
    if (displayQueue == NULL || _locked) { return; }
    _pager->loop();
#    ifdef USE_NEXTION
    nextion.loop();
#    endif
    requestParams_t request;
    if (xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS)) {
        bool pm_result = true;
        pm.on_display_queue(request, pm_result);
        if (pm_result) {
            switch (request.type) {
                case NEWMODE: _swichMode((displayMode_e)request.payload); break;
                case CLOSEPLAYLIST: {
                    dsp.setTextDatum(top_left);
                    player.sendCommand({PR_PLAY, request.payload});
                } break;

                case CLOCK:
                    if (_mode == PLAYER || _mode == SCREENSAVER) { _time(request.payload == 1); }
                    /*#ifdef USE_NEXTION
  if(_mode==TIMEZONE) nextion.localTime(network.timeinfo);
  if(_mode==INFO)     nextion.rssi();
#endif*/
                    break;

                case NEWTITLE: _title(); break;
                case NEWSTATION: _station(); break;
                case NEXTSTATION: _drawNextStationNum(request.payload); break;
                case DRAWPLAYLIST: _drawPlaylist(); break;
                case DRAWVOL: _volume(); break;

                case DBITRATE: {
                    if (_mode == PLAYER) { // csak a lejátszás képernyőn frissíti a bitrateWidgetet
                        char buf[20];
                        snprintf(buf, 20, bitrateFmt, config.station.bitrate);

                        if (_bitrate) { _bitrate->setText(config.station.bitrate == 0 ? "" : buf); }

                        if (_bitratewidget) {
                            _bitratewidget->setFormat(config.configFmt);
                            _bitratewidget->setBitrate(config.station.bitrate);
                        }
                        // Beállítja a csatorna számát a widgeten
                        if (_chbox) { _chbox->setText(config.lastStation(), "Ch:%d."); }
                    }
                } break;

                case CLEARALLBITRATE:
                    if (_mode == PLAYER) {
                        if (_bitratewidget) { _bitratewidget->clearAll(); }     // clearallb
                        if (_namedaywidget) { _namedaywidget->clearNameday(); } // Névnap widget törlése, ha használatban van.
                    }
                    break;

                case AUDIOINFO:
                    if (_heapbar) {
                        _heapbar->lock(!config.store.audioinfo);
                        _heapbar->setValue(player.inBufferFilled());
                    }
                    break;

                case SHOWVUMETER:
                    if (_vuwidget) {
                        _vuwidget->lock(!config.store.vumeter);
                        _layoutChange(player.isRunning());
                    }
                    break;

                case SWITCHVUMODE: applyVuModeChange(); break;

                case SHOWWEATHER:
                    if (_weather) { _weather->lock(!config.store.showweather); }
                    if (!config.store.showweather) {
#    ifndef HIDE_IP
                        if (_ipbox) { _ipbox->setText(config.ipToStr(WiFi.localIP()), iptxtFmt); }
#    endif
                    } else {
                        if (_weather) { _weather->setText(LANG::const_getWeather); } // Üres string
                    }
                    break;

                case NEWWEATHER:
                    if (_weather && timekeeper.weatherBuf) { _weather->setText(timekeeper.weatherBuf); }
                    break;

                case SHOWRSSIMODE:
#    ifndef HIDE_RSSI
                    _applyRssiMode();
#    endif
                    break;

                case BOOTSTRING:
                    if (_bootstring) { _bootstring->setText(config.ssids[request.payload].ssid, LANG::bootstrFmt); }
                    /*#ifdef USE_NEXTION
 char buf[50];
 snprintf(buf, 50, bootstrFmt, config.ssids[request.payload].ssid);
 nextion.bootString(buf);
#endif*/
                    break;

                case WAITFORSD:
                    if (_bootstring) { _bootstring->setText(LANG::const_waitForSD); }
                    break;

                case SDFILEINDEX:
                    if (_mode == SDCHANGE) { _nums->setText(request.payload, "%d"); }
                    break;

                case DSPRSSI:
                    _setRSSI(request.payload);
                    if (_heapbar && config.store.audioinfo) { _heapbar->setValue(player.isRunning() ? player.inBufferFilled() : 0); }
                    break;

                case PSTART: _layoutChange(true); break;
                case PSTOP: _layoutChange(false); break;
                case DSP_START: _start(); break;

                case NEWIP:
#    ifndef HIDE_IP
                    if (_ipbox) { _ipbox->setText(config.ipToStr(WiFi.localIP()), iptxtFmt); }
#    endif
                    break;

                default:
                    break;
                    // check if there are more messages waiting in the Q, in this case break the loop() and go
                    // for another round to evict next message, do not waste time to redraw the screen, etc...
                    if (uxQueueMessagesWaiting(displayQueue)) { return; }
            }
        }
    }

    dsp.loop();
} // loop vége

void Display::_setRSSI(int rssi) {
    if (config.store.rssiAsText && _rssibox) {
        _rssibox->setText(rssi, "%ddBm");
        return;
    }
    if (_wifiwidget) {
        _wifiwidget->setRSSI(rssi);
        return;
    }
    if (!_rssi) { return; }
#    if RSSI_DIGIT
    _rssi->setText(rssi, rssiFmt);
    return;
#    endif
    char rssiG[3];
    int  rssi_steps[] = {RSSI_STEPS};
    if (rssi >= rssi_steps[0]) { strlcpy(rssiG, "\004\006", 3); }
    if (rssi >= rssi_steps[1] && rssi < rssi_steps[0]) { strlcpy(rssiG, "\004\005", 3); }
    if (rssi >= rssi_steps[2] && rssi < rssi_steps[1]) { strlcpy(rssiG, "\004\002", 3); }
    if (rssi >= rssi_steps[3] && rssi < rssi_steps[2]) { strlcpy(rssiG, "\003\002", 3); }
    if (rssi < rssi_steps[3] || rssi >= 0) { strlcpy(rssiG, "\001\002", 3); }
    _rssi->setText(rssiG);
}

void Display::_station() {
    _meta->setAlign(metaConf.widget.align);
    if (config.station.name[0] == '.') {
        _meta->setText(config.station.name + 1);
    } else {
        _meta->setText(config.station.name);
    }

    /*#ifdef USE_NEXTION
    nextion.newNameset(config.station.name);
    nextion.bitrate(config.station.bitrate);
    nextion.bitratePic(ICON_NA);
  #endif*/
}

char* split(char* str, const char* delim) {
    char* dmp = strstr(str, delim);
    if (dmp == NULL) { return NULL; }
    *dmp = '\0';
    return dmp + strlen(delim);
}

void Display::_title() {
    // Ha üres a title, használja a playlistben tárolt nevet.
    if (strlen(config.station.title) == 0) { strlcpy(config.station.title, config.station.name, sizeof(config.station.title)); }
    if (strlen(config.station.title) > 0) {
        char tmpbuf[strlen(config.station.title) + 1];
        strlcpy(tmpbuf, config.station.title, sizeof(tmpbuf));
        char* stitle = split(tmpbuf, " - ");
        if (stitle && _title2) {
            _title1->setText(tmpbuf);
            _title2->setText(stitle);
        } else {
            _title1->setText(config.station.title);
            if (_title2) { _title2->setText(""); }
        }
    } else {
        _title1->setText("");
        if (_title2) { _title2->setText(""); }
    }
    if (player_on_track_change) { player_on_track_change(); }
    pm.on_track_change();
}

void Display::_time(bool redraw) {
    tm displayTime{};
    network_get_timeinfo_snapshot(&displayTime);

#    if LIGHT_SENSOR != 255
    if (config.store.dspon) {
        config.store.brightness = AUTOBACKLIGHT(analogRead(LIGHT_SENSOR));
        config.setBrightness();
    }
#    endif
    if (config.isScreensaver && displayTime.tm_sec % 60 == 0) {
        const int16_t minTop = TFT_FRAMEWDT;
        const int16_t minLeft = TFT_FRAMEWDT;
        const int16_t maxTop = (int16_t)dsp.height() - (int16_t)_clock->height() - (int16_t)TFT_FRAMEWDT;
        const int16_t maxLeft = (int16_t)dsp.width() - (int16_t)_clock->clockWidth() - (int16_t)TFT_FRAMEWDT;

        uint16_t ft = (uint16_t)max<int16_t>(0, minTop);
        uint16_t lt = (uint16_t)max<int16_t>(0, minLeft);

        if (maxTop > minTop) { ft = static_cast<uint16_t>(random(minTop, maxTop + 1)); }
        if (maxLeft > minLeft) { lt = static_cast<uint16_t>(random(minLeft, maxLeft + 1)); }

        _clock->moveTo({lt, ft, 0}); // Az óra új helyre mozgatása beégés megelőzéshez, képernyőn belül tartva.
    }
    _clock->draw(redraw);
    /*#ifdef USE_NEXTION
      nextion.printClock(network.timeinfo);
    #endif*/
}

void Display::_volume() {
#    ifndef HIDE_VOL
    if (_volwidget) { _volwidget->setVolume(config.store.volume); }
#    endif
    if (_mode == VOL) {
        timekeeper.waitAndReturnPlayer(2);
        _nums->setText(config.store.volume, numtxtFmt);
    }
    /*#ifdef USE_NEXTION
      nextion.setVol(config.store.volume, _mode == VOL);
    #endif*/
}

void Display::flip() {
    dsp.flip();
}

void Display::invert() {
    dsp.invert();
}

void Display::setContrast() {}

bool Display::deepsleep() {
#    if defined(DSP_OLED) || BRIGHTNESS_PIN != 255
    dsp.sleep();
    return true;
#    endif
    return false;
}

void Display::wakeup() {
#    if defined(DSP_OLED) || BRIGHTNESS_PIN != 255
    dsp.wake();
#    endif
}

void Display::setBrightnessPercent(uint8_t percent) {
    percent = constrain(percent, 0, 100);
#    if DSP_MODEL == DSP_SSD1322
    uint8_t master = map(percent, 0, 100, 0, 15);
    uint8_t contrast = map(percent, 0, 100, 0, 255);
    dsp.ssd1322_setMasterContrast(master);
    dsp.ssd1322_setContrast(contrast);
#    else
#        if (BRIGHTNESS_PIN != 255)
    analogWrite(BRIGHTNESS_PIN, map(percent, 0, 100, 0, 255));
#        endif
#    endif
}

#    ifdef NAMEDAYS_FILE
void Display::loopNameday(bool force) {
    if (_namedaywidget) { _namedaywidget->draw(force); }
}
#    endif
//============================================================================================================================
#else // !DUMMYDISPLAY
//============================================================================================================================
void Display::init() {
    _createDspTask();
#    ifdef USE_NEXTION
    nextion.begin(true);
#    endif
}
void Display::_start() {
#    ifdef USE_NEXTION
    // nextion.putcmd("page player");
    nextion.start();
#    endif
    config.setTitle(LANG::const_PlReady);
}

void Display::putRequest(displayRequestType_e type, int payload) {
    if (type == DSP_START) { _start(); }
#    ifdef USE_NEXTION
    requestParams_t request;
    request.type = type;
    request.payload = payload;
    nextion.putRequest(request);
#    else
    if (type == NEWMODE) { mode((displayMode_e)payload); }
#    endif
}
//============================================================================================================================
#endif // DUMMYDISPLAY

#ifndef DUMMYDISPLAY
void display_show_maintenance_screen() {
    dsp.initDisplay();
    dsp.wake();
#    if BRIGHTNESS_PIN != 255
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    analogWrite(BRIGHTNESS_PIN, map(100, 0, 100, 0, 255));
#    endif
    dsp.fillScreen(0x0000);
    uint16_t cx = dsp.width() / 2;
    uint16_t cy = dsp.height() / 2;
    dsp.setTextSize(2);
    dsp.setTextDatum(datum_t::middle_center);
    dsp.setTextColor(0xFFFF, 0x0000);
    dsp.drawString("LittleFS Serial Service", cx, cy - 20);
    dsp.drawString("Maintenance mode active", cx, cy + 14);
}
#else
void display_show_maintenance_screen() {}
#endif
