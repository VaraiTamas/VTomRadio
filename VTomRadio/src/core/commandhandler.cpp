// "nameday"
#include "options.h"
#include "commandhandler.h"
#include "player.h"
#include "display.h"
#include "netserver.h"
#include "config.h"
#include "clock_tts.h"
#include "controls.h"
#include "fonts.h"
//#include "../displays/display_select.h"
#include "../plugins/backlight/backlight.h"

#if DSP_MODEL == DSP_DUMMY
#    define DUMMYDISPLAY
#endif

CommandHandler cmd;

static bool parseClockTtsTimeToMinutes(const char* value, uint16_t& outMinutes) {
    if (!value) return false;
    int h = 0;
    int m = 0;
    if (sscanf(value, "%d:%d", &h, &m) != 2) return false;
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    outMinutes = static_cast<uint16_t>(h * 60 + m);
    return true;
}

bool CommandHandler::exec(const char* command, const char* value, uint8_t cid) {
    Serial.printf("commandhandler.cpp--> command: %s, value: %s, cId: %d \n", command, value, cid);
    if (strEquals(command, "start")) {
        // Serial.printf("commandhandler.cpp--> START \n");
        player.sendCommand({PR_PLAY, config.lastStation()});
        return true;
    }
    if (strEquals(command, "stop")) {
        // Serial.printf("commandhandler.cpp--> STOP \n");
        player.sendCommand({PR_STOP, 0});
        return true;
    }
    if (strEquals(command, "toggle")) {
        //  Serial.printf("commandhandler.cpp-->  player.toggle() \"toggle\" \n");
        player.toggle();
        /*
       uint32_t pos = strtoul(value, nullptr, 10);
       if (pos == 1) {
         player.toggle();
       } else {
         player.toggleFromWeb(pos);
       }
         */
        return true;
    }
    if (strEquals(command, "prev")) {
        player.prev();
        return true;
    }
    if (strEquals(command, "next")) {
        player.next();
        return true;
    }
    if (strEquals(command, "volm")) {
        player.stepVol(false);
        return true;
    }
    if (strEquals(command, "volp")) {
        player.stepVol(true);
        return true;
    }
#ifdef USE_SD
    if (strEquals(command, "mode")) {
        config.changeMode(atoi(value));
        return true;
    }
#endif
    if (strEquals(command, "reset") && cid == 0) {
        config.reset();
        return true;
    }
    // if (strEquals(command, "ballance")) { config.setBalance(atoi(value)); return true; }
    if (strEquals(command, "playstation") || strEquals(command, "play")) {
        // Serial.printf("commandhandler.cpp--> command: %s\n", command);
        int id = atoi(value);
        if (id < 1) { id = 1; }
        uint16_t cs = config.playlistLength();
        if (id > cs) { id = cs; }
        player.sendCommand({PR_PLAY, id});
        return true;
    }
    if (strEquals(command, "vol")) {
        int v = atoi(value);
        config.store.volume = v < 0 ? 0 : (v > 100 ? 100 : v); // Módosítás "vol_step"
        player.setVol(v);
        return true;
    }
    if (strEquals(command, "dspon")) {
        config.setDspOn(atoi(value) != 0);
        return true;
    }
    if (strEquals(command, "dim")) {
        int d = atoi(value);
        config.store.brightness = (uint8_t)(d < 0 ? 0 : (d > 100 ? 100 : d));
        config.setBrightness(true);
        return true;
    }

    if (strEquals(command, "clearspiffs")) {
        config.littlefsCleanup();
        config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
        return true;
    }
    /*********************************************/
    /****************** WEBSOCKET ****************/
    /*********************************************/
    if (strEquals(command, "getindex")) {
        netserver.requestOnChange(GETINDEX, cid);
        return true;
    }

    if (strEquals(command, "getsystem")) {
        netserver.requestOnChange(GETSYSTEM, cid);
        return true;
    }
    if (strEquals(command, "getscreen")) {
        netserver.requestOnChange(GETSCREEN, cid);
        return true;
    }
    if (strEquals(command, "gettimezone")) {
        netserver.requestOnChange(GETTIMEZONE, cid);
        return true;
    }
    if (strEquals(command, "getcontrols")) {
        netserver.requestOnChange(GETCONTROLS, cid);
        return true;
    }
    if (strEquals(command, "getweather")) {
        netserver.requestOnChange(GETWEATHER, cid);
        return true;
    }
    if (strEquals(command, "getactive")) {
        netserver.requestOnChange(GETACTIVE, cid);
        return true;
    }
    if (strEquals(command, "newmode")) {
        config.newConfigMode = atoi(value);
        netserver.requestOnChange(CHANGEMODE, cid);
        return true;
    }

    if (strEquals(command, "invertdisplay")) {
        config.saveValue(&config.store.invertdisplay, static_cast<bool>(atoi(value)));
        display.invert();
        return true;
    }
    if (strEquals(command, "numplaylist")) {
        config.saveValue(&config.store.numplaylist, static_cast<bool>(atoi(value)));
        display.putRequest(NEWMODE, CLEAR);
        display.putRequest(NEWMODE, PLAYER);
        return true;
    }
    if (strEquals(command, "playlistmovingcursor")) {
        config.saveValue(&config.store.playlistMovingCursor, static_cast<bool>(atoi(value)));
        display.putRequest(NEWMODE, CLEAR);
        display.putRequest(NEWMODE, PLAYER);
        return true;
    }
    if (strEquals(command, "directchannelchange")) {
        config.saveValue(&config.store.directChannelChange, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "stationslistreturntime")) {
        uint8_t v = static_cast<uint8_t>(atoi(value));
        if (v < 1) v = 1;
        config.saveValue(&config.store.stationsListReturnTime, v);
        return true;
    }
    if (strEquals(command, "fliptouch")) {
        config.saveValue(&config.store.fliptouch, static_cast<bool>(atoi(value)));
       //        flipTS();
        return true;
    }
    if (strEquals(command, "dbgtouch")) {
        config.saveValue(&config.store.dbgtouch, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "xtouchmirroring")) {
        config.saveValue(&config.store.xTouchMirroring, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "ytouchmirroring")) {
        config.saveValue(&config.store.yTouchMirroring, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "flipscreen")) {
        config.saveValue(&config.store.flipscreen, static_cast<bool>(atoi(value)));
        display.flip();
        display.putRequest(NEWMODE, CLEAR);
        display.putRequest(NEWMODE, PLAYER);
        return true;
    }
    if (strEquals(command, "brightness")) {
        if (!config.store.dspon) { netserver.requestOnChange(DSPON, 0); }
        config.store.brightness = static_cast<uint8_t>(atoi(value));
        config.setBrightness(true);
        if (BRIGHTNESS_PIN != 255 && config.store.fadeEnabled) { // WEB UI backlight plugin
            if (backlightPlugin.isDimmed() || backlightPlugin.isFading())
                backlightPlugin.wake();
            else
                backlightPlugin.activity();
        }
        return true;
    }
    if (strEquals(command, "screenon")) {
        config.setDspOn(static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "contrast")) {
        config.saveValue(&config.store.contrast, static_cast<uint8_t>(atoi(value)));
        display.setContrast();
        return true;
    }
    if (strEquals(command, "screensaverenabled")) {
        config.enableScreensaver(static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "screensavertimeout")) {
        config.setScreensaverTimeout(static_cast<uint16_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "screensaverblank")) {
        config.setScreensaverBlank(static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "screensaverplayingenabled")) {
        config.setScreensaverPlayingEnabled(static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "screensaverplayingtimeout")) {
        config.setScreensaverPlayingTimeout(static_cast<uint16_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "screensaverplayingblank")) {
        config.setScreensaverPlayingBlank(static_cast<bool>(atoi(value)));
        return true;
    }
    /***** AUTO FADE *****/
    if (cmd.strEquals(command, "fadeenabled")) {
        config.saveValue(&config.store.fadeEnabled, static_cast<uint8_t>(atoi(value)), true, false);
        return true;
    }
    if (cmd.strEquals(command, "fadestartdelay")) {
        config.saveValue(&config.store.fadeStartDelay, static_cast<uint16_t>(atoi(value)), true, false);
        return true;
    }
    if (cmd.strEquals(command, "fadetarget")) {
        uint8_t target = atoi(value);
        if (target > 100) target = 100;
        config.saveValue(&config.store.fadeTarget, target, true, false);
        return true;
    }
    if (cmd.strEquals(command, "fadestep")) {
        uint8_t step = atoi(value);
        if (step > 100) step = 100;
        config.saveValue(&config.store.fadeStep, step, true, false);
        return true;
    }
    /*********************/
    if (strEquals(command, "abuff")) {
        config.saveValue(&config.store.abuff, static_cast<uint16_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "watchdog")) {
        config.saveValue(&config.store.watchdog, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "stallwatchdog")) {
        config.saveValue(&config.store.stallWatchdog, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "nameday")) {                                         // "nameday"  itt veszi át a webről a toggle értékét.
        display.putRequest(CLEARALLBITRATE);                                     // Törli mindkét bitratewidget és a nameday területet.
        config.saveValue(&config.store.nameday, static_cast<bool>(atoi(value))); // Elmenti a gomb beállítását
        display.putRequest(DBITRATE);
        return true;
    }
    if (strEquals(command, "clocktts")) {
        bool enabled = static_cast<bool>(atoi(value));
        config.saveValue(&config.store.clockTtsEnabled, enabled);
        clock_tts_enable(enabled);
        return true;
    }
    if (strEquals(command, "clockttslang")) {
        if (
            strEquals(value, "EN") || strEquals(value, "HU") || strEquals(value, "PL") || strEquals(value, "NL") || strEquals(value, "DE")
            || strEquals(value, "RU") || strEquals(value, "RO") || strEquals(value, "FR") || strEquals(value, "GR")
        ) {
            config.saveValue(config.store.clockTtsLanguage, value, sizeof(config.store.clockTtsLanguage));
            clock_tts_set_language(config.store.clockTtsLanguage);
            return true;
        }
        return false;
    }
    if (strEquals(command, "clockttsinterval")) {
        uint16_t interval = static_cast<uint16_t>(atoi(value));
        interval = constrain(interval, 1, 1440);
        config.saveValue(&config.store.clockTtsIntervalMinutes, interval);
        clock_tts_set_interval(interval);
        return true;
    }
    if (strEquals(command, "clockttsnostream")) {
        bool enabled = static_cast<bool>(atoi(value));
        config.saveValue(&config.store.clockTtsOnlyWhenNoStream, enabled);
        clock_tts_set_only_when_no_stream(enabled);
        return true;
    }
    if (strEquals(command, "clockttsquietenabled")) {
        bool enabled = static_cast<bool>(atoi(value));
        config.saveValue(&config.store.clockTtsQuietHoursEnabled, enabled);
        clock_tts_set_quiet_hours(enabled, config.store.clockTtsQuietFromMinutes, config.store.clockTtsQuietToMinutes);
        return true;
    }
    if (strEquals(command, "clockttsquietfrom")) {
        uint16_t mins = 0;
        if (!parseClockTtsTimeToMinutes(value, mins)) return false;
        config.saveValue(&config.store.clockTtsQuietFromMinutes, mins);
        clock_tts_set_quiet_hours(config.store.clockTtsQuietHoursEnabled, config.store.clockTtsQuietFromMinutes, config.store.clockTtsQuietToMinutes);
        return true;
    }
    if (strEquals(command, "clockttsquietto")) {
        uint16_t mins = 0;
        if (!parseClockTtsTimeToMinutes(value, mins)) return false;
        config.saveValue(&config.store.clockTtsQuietToMinutes, mins);
        clock_tts_set_quiet_hours(config.store.clockTtsQuietHoursEnabled, config.store.clockTtsQuietFromMinutes, config.store.clockTtsQuietToMinutes);
        return true;
    }
    if (strEquals(command, "dateformat")) {
        config.saveValue(&config.store.dateFormat, static_cast<uint8_t>(atoi(value)));
        display.putRequest(CLOCK, 1);
        return true;
    }
    if (strEquals(command, "clockfont")) {
        uint8_t style = static_cast<uint8_t>(atoi(value));
        if (style > CLOCKFONT_STYLE_CALIBRI) { style = CLOCKFONT_STYLE_DIGI7; }
        config.saveValue(&config.store.clockFontStyle, style);
        if (style != CLOCKFONT_STYLE_DIGI7 && config.store.clockFontMono) {
            config.saveValue(&config.store.clockFontMono, false);
        }
        setClockFontStyle(style);
        display.putRequest(CLOCK, 1);
        netserver.requestOnChange(GETTIMEZONE, cid);
        return true;
    }
    if (strEquals(command, "clockfontmono")) {
        bool enabled = static_cast<bool>(atoi(value));
        if (config.store.clockFontStyle != CLOCKFONT_STYLE_DIGI7) { enabled = false; }
        config.saveValue(&config.store.clockFontMono, enabled);
        display.putRequest(CLOCK, 1);
        netserver.requestOnChange(GETTIMEZONE, cid);
        return true;
    }
    if (strEquals(command, "clockampm")) {
        bool enabled = static_cast<bool>(atoi(value));
        config.saveValue(&config.store.clockAmPmStyle, enabled);
        display.putRequest(CLOCK, 1);
        netserver.requestOnChange(GETTIMEZONE, cid);
        return true;
    }
    if (strEquals(command, "tzh")) {
        config.saveValue(&config.store.tzHour, static_cast<int8_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "tzm")) {
        config.saveValue(&config.store.tzMin, static_cast<int8_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "sntp2")) {
        config.saveValue(config.store.sntp2, value, 35, false);
        return true;
    }
    if (strEquals(command, "sntp1")) {
        config.setSntpOne(value);
        return true;
    }
    if (strEquals(command, "timeint")) {
        uint16_t interval = static_cast<uint16_t>(atoi(value));
        if (interval < 1) { interval = 1; }
        config.saveValue(&config.store.timeSyncInterval, interval);
        return true;
    }
    if (strEquals(command, "timeintrtc")) {
        uint16_t intervalRtc = static_cast<uint16_t>(atoi(value));
        if (intervalRtc < 1) { intervalRtc = 1; }
        config.saveValue(&config.store.timeSyncIntervalRTC, intervalRtc);
        return true;
    }
    if (strEquals(command, "irtlp")) {
        setIRTolerance(static_cast<uint8_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "oneclickswitching")) {
        config.saveValue(&config.store.skipPlaylistUpDown, static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "encodersindependent")) {
        const bool dualEncodersConfigured = (ENC_BTNL != 255 && ENC_BTNR != 255) && (ENC2_BTNL != 255 && ENC2_BTNR != 255);
        config.saveValue(&config.store.encodersIndependent, dualEncodersConfigured && static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "showweather")) {
        config.setShowweather(static_cast<bool>(atoi(value)));
        return true;
    }
    if (strEquals(command, "rssiastext")) {
        config.saveValue(&config.store.rssiAsText, static_cast<bool>(atoi(value)));
        display.putRequest(SHOWRSSIMODE);
        return true;
    }
    if (strEquals(command, "lat")) {
        config.saveValue(config.store.weatherlat, value, 10, false);
        return true;
    }
    if (strEquals(command, "lon")) {
        config.saveValue(config.store.weatherlon, value, 10, false);
        return true;
    }
    if (strEquals(command, "key")) {
        config.setWeatherKey(value);
        return true;
    }
    if (strEquals(command, "wint")) {
        uint16_t weatherInterval = static_cast<uint16_t>(atoi(value));
        if (weatherInterval < 1) { weatherInterval = 1; }
        config.saveValue(&config.store.weatherSyncInterval, weatherInterval);
        return true;
    }
    if (strEquals(command, "volume")) { // Ha weben állítom a hangerő csúszkát itt kapja meg az értéket.
        player.setVol(static_cast<uint8_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "sdpos")) {
        ;
        uint32_t sdpos = static_cast<uint32_t>(atoi(value));
        config.setSDpos(sdpos);
        return true;
    }
    if (strEquals(command, "snuffle")) { // véletlen lejátszás
        config.setSnuffle(strcmp(value, "true") == 0);
        return true;
    }
    if (strEquals(command, "balance")) {
        config.setBalance(static_cast<int8_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "reboot")) {
        ESP.restart();
        return true;
    }
    if (strEquals(command, "boot")) {
        ESP.restart();
        return true;
    }
    if (strEquals(command, "format")) {
        LittleFS.format();
        ESP.restart();
        return true;
    }
    if (strEquals(command, "submitplaylist")) {
        player.sendCommand({PR_STOP, 0});
        return true;
    }

#if IR_PIN != 255
    if (strEquals(command, "irbtn")) { // Gombok  value: 0-18 
        config.setIrBtn(atoi(value));
        return true;
    }
    if (strEquals(command, "chkid")) { // A három IR bank chkid, value: 0, 1 vagy 2
        IRCommand ircmd = {};
        ircmd.irBankId = static_cast<uint8_t>(atoi(value));
        ircmd.hasBank = true;
        ircmd.hasBtnId = false; // Nem szükséges az index átadása, mert a tanulás során már beállításra kerül a config.irBtnId változó.
        xQueueSend(irQueue, &ircmd, 0);
        Serial.printf("commandhandler.cpp--> xQueueSend chkid: %d\n", ircmd.irBankId);
        return true;
    }
    if (strEquals(command, "irclr")) { // Bank törlés. --> command: irclr
        config.ircodes.irVals[config.irBtnId][static_cast<uint8_t>(atoi(value))] = 0;
        Serial.printf("commandhandler.cpp--> Bank törlés--> config.irBtnId: %d, chkid: %d\n", config.irBtnId, static_cast<uint8_t>(atoi(value)));
        return true;
    }
#endif
    if (strEquals(command, "reset")) {
        config.resetSystem(value, cid);
        return true;
    }

    if (strEquals(command, "smartstart")) {
        uint8_t ss = atoi(value) == 1 ? 1 : 2;
        if (!player.isRunning() && ss == 1) { ss = 0; }
        config.setSmartStart(ss);
        return true;
    }
    if (strEquals(command, "audioinfo")) {
        config.saveValue(&config.store.audioinfo, static_cast<bool>(atoi(value)));
        display.putRequest(AUDIOINFO);
        return true;
    }
    if (strEquals(command, "vumeter")) {
        config.saveValue(&config.store.vumeter, static_cast<bool>(atoi(value)));
        display.putRequest(SHOWVUMETER);
        return true;
    }
    if (strEquals(command, "vupeak")) {
        config.saveValue(&config.store.vuPeak, static_cast<bool>(atoi(value)));
        display.putRequest(SHOWVUMETER);
        return true;
    }
    if (strEquals(command, "vubox")) {
        config.setVuBidirectional(static_cast<bool>(atoi(value)));
        display.putRequest(SWITCHVUMODE);
        return true;
    }
    if (strEquals(command, "softap")) {
        config.saveValue(&config.store.softapdelay, static_cast<uint8_t>(atoi(value)));
        return true;
    }
    if (strEquals(command, "mdnsname")) {
        config.saveValue(config.store.mdnsname, value, MDNS_LENGTH);
        return true;
    }
    if (strEquals(command, "rebootmdns")) {
        if (strlen(config.store.mdnsname) > 0) {
            snprintf(config.tmpBuf, sizeof(config.tmpBuf), "{\"redirect\": \"http://%s.local/settings.html\"}", config.store.mdnsname);
        } else {
            snprintf(config.tmpBuf, sizeof(config.tmpBuf), "{\"redirect\": \"http://%s/settings.html\"}", config.ipToStr(WiFi.localIP()));
        }
        websocket.text(cid, config.tmpBuf);
        delay(500);
        ESP.restart();
        return true;
    }

    return false;
}