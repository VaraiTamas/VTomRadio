#include "options.h"
#include <Wire.h>
#include "config.h"
#include "display.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "controls.h"
#include "timekeeper.h"
#include "rtcsupport.h"
#include "../displays/tools/language.h"
#include "driver/rtc_io.h"
#ifdef USE_SD
#    include "sdmanager.h"
#endif
#ifdef USE_NEXTION
#    include "../displays/nextion.h"
#endif
#include <cstddef>
#include <cctype>

#if DSP_MODEL == DSP_DUMMY
#    define DUMMYDISPLAY
#endif

Config config;
#if IR_PIN != 255
QueueHandle_t irQueue = nullptr;
#endif

namespace {

bool equalsIgnoreCase(const char* a, const char* b) {
    if (!a || !b) { return false; }
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b))) { return false; }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

char* trimInPlace(char* s) {
    if (!s) { return s; }
    while (*s && isspace(static_cast<unsigned char>(*s))) { ++s; }
    if (*s == '\0') { return s; }
    char* end = s + strlen(s) - 1;
    while (end > s && isspace(static_cast<unsigned char>(*end))) {
        *end = '\0';
        --end;
    }
    return s;
}

void color565ToRgb(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
    g = static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
    b = static_cast<uint8_t>((color & 0x1F) * 255 / 31);
}

uint8_t clampChannel(long value) {
    if (value < 0) { return 0; }
    if (value > 255) { return 255; }
    return static_cast<uint8_t>(value);
}

void parseVersionTriplet(const char* ver, uint8_t& a, uint8_t& b, uint8_t& c) {
    a = 0;
    b = 0;
    c = 0;
    if (!ver || !*ver) { return; }

    char tmp[32];
    strlcpy(tmp, ver, sizeof(tmp));

    char* saveptr = nullptr;
    char* tok = strtok_r(tmp, ".", &saveptr);
    if (!tok) { return; }
    a = clampChannel(strtol(tok, nullptr, 10));

    tok = strtok_r(nullptr, ".", &saveptr);
    if (!tok) { return; }
    b = clampChannel(strtol(tok, nullptr, 10));

    tok = strtok_r(nullptr, ".", &saveptr);
    if (!tok) { return; }
    c = clampChannel(strtol(tok, nullptr, 10));
}

} // namespace

void u8fix(char* src) { // Ha az utolsó tőbbájtos karakter (ékezetes) utolsó bájtja hiányzik akkor az elejét levágja.
    char last = src[strlen(src) - 1];
    if ((uint8_t)last >= 0xC2) { src[strlen(src) - 1] = '\0'; }
}

bool Config::_isFSempty() {
    // Base names without .gz — accepts both compressed and plain uploads
    const char*   reqiredFiles[] = {"dragpl.js",   "ir.css",    "irrecord.html", "ir.js",        "logo.svg",  "options.html",
                                    "player.html", "script.js", "style.css",     "updform.html", "theme.css", "theme-editor.html"};
    const uint8_t reqiredFilesSize = 12;
    char          fullpath[32];
    if (LittleFS.exists("/www/settings.html")) { LittleFS.remove("/www/settings.html"); }
    if (LittleFS.exists("/www/update.html")) { LittleFS.remove("/www/update.html"); }
    if (LittleFS.exists("/www/index.html")) { LittleFS.remove("/www/index.html"); }
    if (LittleFS.exists("/www/ir.html")) { LittleFS.remove("/www/ir.html"); }
    if (LittleFS.exists("/www/elogo.png")) { LittleFS.remove("/www/elogo.png"); }
    if (LittleFS.exists("/www/elogo84.png")) { LittleFS.remove("/www/elogo84.png"); }
    for (uint8_t i = 0; i < reqiredFilesSize; i++) {
        snprintf(fullpath, sizeof(fullpath), "/www/%s", reqiredFiles[i]);
        bool plain = LittleFS.exists(fullpath);
        snprintf(fullpath, sizeof(fullpath), "/www/%s.gz", reqiredFiles[i]);
        bool gz = LittleFS.exists(fullpath);
        if (!plain && !gz) {
            Serial.printf("[FS] Missing: %s(.gz)\n", fullpath);
            return true;
        }
    }
    return false;
}

void Config::init() {
    sdResumePos = 0;
    /*----- I2C init -----*/
#if (RTC_MODULE == DS3231) || (TS_MODEL == TS_MODEL_FT6X36) || (TS_MODEL == TS_MODEL_GT911) || (TS_MODEL == TS_MODEL_AXS15231B)
    Serial.println("\n[INIT] Initializing I2C...");
#    if (TS_MODEL == TS_MODEL_FT6X36) && (TS_RST != 255)
    // FT6336U needs a hardware reset before it will respond on I2C
    Serial.printf("[INIT] FT6X36 reset on GPIO %d\n", TS_RST);
    pinMode(TS_RST, OUTPUT);
    digitalWrite(TS_RST, LOW);
    delay(20);
    digitalWrite(TS_RST, HIGH);
    delay(200);
#    endif
    Wire.begin(TS_SDA, TS_SCL);
    Wire.setClock(400000);
    Serial.println("[INIT] Scanning I2C @400kHz...");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found: 0x%02X\n", addr);
            ++found;
        }
    }
    if (found == 0) { Serial.println("[INIT] I2C scan found no devices."); }
    Serial.println("[INIT] I2C scan complete.");
#endif
    // DLNA modplus
#ifdef USE_DLNA
    isBooting = true;
    resumeAfterModeChange = false;
#endif
    // DLNA modplus
    screensaverTicks = 0;
    screensaverPlayingTicks = 0;
    newConfigMode = 0;
    isScreensaver = false;
    memset(tmpBuf, 0, BUFLEN);
#if RTCSUPPORTED
    _rtcFound = false;
    BOOTLOG("RTC begin(SDA=%d,SCL=%d)", RTC_SDA, RTC_SCL);
    if (rtc.init()) {
        BOOTLOG("done");
        _rtcFound = true;
    } else {
        BOOTLOG("[ERROR] - Couldn't find RTC");
    }
#endif
    emptyFS = true;
#if IR_PIN != 255
    irBtnId = -1;
#endif
#if defined(SD_SPIPINS)
#    if !defined(SD_SPIPINS)
    SDSPI.begin();
#    else
    SDSPI.begin(SD_SPIPINS); // SCK, MISO, MOSI
#    endif
#endif
    eepromRead(EEPROM_START, store);

#ifdef USE_DLNA
    if (store.lastPlayedSource == PL_SRC_DLNA) {
        store.playlistSource = PL_SRC_DLNA;
    } else {
        store.playlistSource = PL_SRC_WEB;
    }
#endif
    bootInfo();
    if (store.config_set != 0) { setDefaults(); }
    if (store.version > CONFIG_VERSION) {
        saveValue(&store.version, (uint16_t)CONFIG_VERSION, true, true);
    } else {
        while (store.version != CONFIG_VERSION) { _setupVersion(); }
    }
    if (store.vuPeakInitMarker != 0xA5) {
        saveValue(&store.vuPeak, true, false);
        saveValue(&store.vuPeakInitMarker, static_cast<uint8_t>(0xA5));
    }
    if (store.clockFontStyle > CLOCKFONT_STYLE_CALIBRI) { saveValue(&store.clockFontStyle, static_cast<uint8_t>(CLOCKFONT_STYLE)); }
    if (store.clockFontStyle != CLOCKFONT_STYLE_DIGI7 && store.clockFontMono) { saveValue(&store.clockFontMono, false); }
    if (store.dateFormat > 4) { saveValue(&store.dateFormat, static_cast<uint8_t>(0)); }
    BOOTLOG("CONFIG_VERSION\t%d", store.version);

    store.play_mode = store.play_mode & 0b11;
    // DLNA modplus
#ifdef USE_DLNA
#else
    if (store.play_mode > 1) { store.play_mode = PM_WEB; }
#endif
    // DLNA modplus
    _initHW();
    if (!LittleFS.begin(false)) {
        Serial.println("##[ERROR]# LittleFS Mount Failed, formatting...");

        LittleFS.format();

        if (!LittleFS.begin()) {
            Serial.println("##[FATAL]# LittleFS still failed!");
            return;
        }
    }
    BOOTLOG("LittleFS mounted");
    bool themeLoaded = loadThemeFromFile();
    BOOTLOG("Theme file %s", themeLoaded ? "loaded" : "not found or invalid, using defaults");
    emptyFS = _isFSempty();
    if (emptyFS) {
        BOOTLOG("LittleFS is empty!");
    } else {

        Serial.println("---- CHECK ----");
        File f = LittleFS.open("/logo2.png");
        Serial.println(f ? "OPEN OK" : "OPEN FAIL");
        // DEBUG: Fájlok listázása
        File root = LittleFS.open("/");
        File file = root.openNextFile();

        while (file) {
            Serial.print("FILE RAW: [");
            Serial.print(file.name());
            Serial.println("]");
            file = root.openNextFile();
        }
    }

    ssidsCount = 0;
#ifdef USE_SD
    _SDplaylistFS = getMode() == PM_SDCARD ? &sdman : (true ? &LittleFS : _SDplaylistFS);
#else
    _SDplaylistFS = &LittleFS;
#endif
    _bootDone = false;
    setTimeConf();

#ifdef USE_DLNA
    isBooting = false;
#endif

#if PWR_AMP != 255 // "PWR_AMP"
    pinMode(PWR_AMP, OUTPUT);
    digitalWrite(PWR_AMP, HIGH);
#endif
}

void Config::_setupVersion() {
    uint16_t currentVersion = store.version;
    switch (currentVersion) {
        case 0: saveValue(&store.playlistMovingCursor, false); break;
        case 1: saveValue(&store.encodersIndependent, false); break;
        case 2: saveValue(&store.rssiAsText, false); break;
        case 3: break;
        case 4: saveValue(&store.serialLittlefsEnabled, true); break;
    }
    currentVersion++;
    saveValue(&store.version, currentVersion);
}

void Config::changeMode(int newmode) { // DLNA mod
    // Serial.printf("Config.cpp-->changeMode() newmode: %d", newmode);
#ifdef USE_SD
    // Encoder dupla klikk (paraméter nélküli hívás)
    if (newmode == -1) {
        // DLNA nem választható encoderről
        newmode = (getMode() == PM_SDCARD) ? PM_WEB : PM_SDCARD;
    }

    // 🔒 biztonsági ellenőrzés
    if (newmode < 0 || newmode >= 2) { // 0 --> radio; 1 --> SD; 2 --> DLNA
        Serial.printf("##[ERROR]# changeMode invalid newmode: %d\n", newmode);
        return;
    }

    bool pir = player.isRunning();

    if (SDC_CS == 255 && newmode == PM_SDCARD) { return; }

    if (network.status == SOFT_AP || display.mode() == LOST) {
        saveValue(&store.play_mode, (uint8_t)PM_SDCARD);
        delay(50);
        ESP.restart();
    }

    /* === SD only when explicitly requested === */
    if (newmode == PM_SDCARD) {
        if (!sdman.ready) {
            if (!sdman.start()) {
                Serial.println("##[ERROR]# SD Not Found");
                netserver.requestOnChange(GETPLAYERMODE, 0);
                return;
            }
        }
    }

    /* === set mode === */
    store.play_mode = (playMode_e)newmode;
    saveValue(&store.play_mode, (uint8_t)store.play_mode, true, true);

    /* === filesystem binding === */
    if (getMode() == PM_SDCARD) {
        _SDplaylistFS = &sdman;
    } else {
        _SDplaylistFS = &LittleFS; // WEB + DLNA
    }

    /* === SD specific actions === */
    if (getMode() == PM_SDCARD) {
        if (pir) { player.sendCommand({PR_STOP, 0}); }
        display.putRequest(NEWMODE, SDCHANGE);
        delay(50);
    } else {
        sdman.stop(); // WEB + DLNA → SD off
    }

    if (!_bootDone) { return; }

    initPlaylistMode();

    if (pir) {
#    ifdef USE_DLNA
        uint16_t st = (getMode() == PM_SDCARD) ? store.lastSdStation : (store.playlistSource == PL_SRC_DLNA ? store.lastDlnaStation : store.lastStation);
#    else
        uint16_t st = (getMode() == PM_SDCARD) ? store.lastSdStation : store.lastStation;
        player.sendCommand({PR_PLAY, st});
#    endif
    }

    netserver.resetQueue();
    netserver.requestOnChange(GETINDEX, 0);

    display.resetQueue();
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(NEWSTATION);
#endif
}

void Config::initSDPlaylist() {
#ifdef USE_SD
    // bool doIndex = !sdman.exists(INDEX_SD_PATH); // "módosítás"
    // if(doIndex) sdman.indexSDPlaylist();
    //  Mindig legyen indexelés az SD kártyán.
    sdman.indexSDPlaylist();
    if (SDPLFS()->exists(INDEX_SD_PATH)) {
        File index = SDPLFS()->open(INDEX_SD_PATH, "r");
        // if(doIndex){
        lastStation(_randomStation());
        sdResumePos = 0;
        // }
        index.close();
    }
#endif // #ifdef USE_SD
}

bool Config::littlefsCleanup() {
    bool ret = (LittleFS.exists(PLAYLIST_SD_PATH)) || (LittleFS.exists(INDEX_SD_PATH)) || (LittleFS.exists(INDEX_PATH));
    if (LittleFS.exists(PLAYLIST_SD_PATH)) { LittleFS.remove(PLAYLIST_SD_PATH); }
    if (LittleFS.exists(INDEX_SD_PATH)) { LittleFS.remove(INDEX_SD_PATH); }
    if (LittleFS.exists(INDEX_PATH)) { LittleFS.remove(INDEX_PATH); }
    return ret;
}

void Config::waitConnection() {
#if I2S_DOUT == 255
    return;
#endif
    while (!player.connproc) { vTaskDelay(50); }
    vTaskDelay(500);
}

char* Config::ipToStr(IPAddress ip) {
    snprintf(ipBuf, 20, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return ipBuf;
}

bool Config::prepareForPlaying(uint16_t stationId) {
    setDspOn(1);
    vuRefLevel = 0;
    screensaverTicks = SCREENSAVERSTARTUPDELAY;
    screensaverPlayingTicks = SCREENSAVERSTARTUPDELAY;
    if (getMode() != PM_SDCARD) { display.putRequest(PSTOP); }
    if (!loadStation(stationId)) { return false; }
    setTitle(LANG::const_PlConnect); // inen van a connect felirat a kijelzőn
    station.bitrate = 0;
    setBitrateFormat(BF_UNKNOWN);
    display.putRequest(DBITRATE);
    netserver.requestOnChange(BITRATE, 0);
    display.putRequest(NEWSTATION);
    display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(STATION, 0);
    netserver.requestOnChange(MODE, 0);
    netserver.loop();
    netserver.loop();
    if (store.smartstart != 2) { setSmartStart(0); }
    return true;
}

void Config::configPostPlaying(uint16_t stationId) { // DLNA mod
    if (getMode() == PM_SDCARD) {
        saveValue(&store.lastSdStation, stationId);
    }
#ifdef USE_DLNA
    else if (store.playlistSource == PL_SRC_DLNA) {
        saveValue(&store.lastDlnaStation, stationId);
    }
#endif
    else {
        saveValue(&store.lastStation, stationId);
    }

    if (store.smartstart != 2) { setSmartStart(1); }
    netserver.requestOnChange(MODE, 0);
    display.putRequest(PSTART);
}

void Config::setSDpos(uint32_t val) {
    if (getMode() == PM_SDCARD) {
        sdResumePos = 0; // ha kézzel állítasz pozíciót, ne legyen régi resume
        if (!player.isRunning()) {
            config.sdResumePos = val - player.sd_min;
        } else {
            player.setAudioFilePosition(val - player.sd_min); // futó lejátszásnál seek webről
        }
    }
}

void Config::initPlaylistMode() {
    uint16_t _lastStation = 0;

#ifdef USE_SD
    if (getMode() == PM_SDCARD) {
        if (!sdman.start()) {
            changeMode(PM_WEB);
            return;
        }
        initSDPlaylist();
        uint16_t cs = playlistLength();
        _lastStation = store.lastSdStation;
        if (_lastStation == 0 && cs > 0) { _lastStation = _randomStation(); }
    } else
#endif
    {

#ifdef USE_DLNA
        if (store.playlistSource == PL_SRC_DLNA) {

            if (LittleFS.exists(PLAYLIST_DLNA_PATH)) { initDLNAPlaylist(); }

            uint16_t cs = playlistLength();

            // ⬇️ DLNA indexet CSAK innen vesszük
            _lastStation = store.lastDlnaStation;
            if (_lastStation == 0 && cs > 0) { _lastStation = 1; }

        } else
#endif
        {
            initPlaylist();
            uint16_t cs = playlistLength();
            _lastStation = store.lastStation;
            if (_lastStation == 0 && cs > 0) { _lastStation = 1; }
#if defined(ALWAYS_START_FROM_FIRST)
            if (cs > 0) _lastStation = 1;
#endif
        }
    }

    // ⬇️ EGYSZER
    lastStation(_lastStation);
    loadStation(_lastStation);

    _bootDone = true;
}

void Config::_initHW() {
    loadTheme();
#if IR_PIN != 255
    eepromRead(EEPROM_START_IR, ircodes);
#endif
#if BRIGHTNESS_PIN != 255
    gpio_hold_dis((gpio_num_t)BRIGHTNESS_PIN); // ← add (MB)
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    // Keep backlight off during display controller init to avoid boot flash.
    analogWrite(BRIGHTNESS_PIN, 0);
#endif
}

uint16_t Config::color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void Config::loadTheme() {
    _setDefaultTheme();
}

void Config::_setDefaultTheme() {
    theme.background = color565(COLOR_BACKGROUND);
    theme.meta = color565(COLOR_STATION_NAME);
    theme.metabg = color565(COLOR_STATION_BG);
    theme.metafill = color565(COLOR_STATION_FILL);
    theme.title1 = color565(COLOR_SNG_TITLE_1);
    theme.title2 = color565(COLOR_SNG_TITLE_2);
    theme.digit = color565(COLOR_DIGITS);
    theme.div = color565(COLOR_DIVIDER);
    /*----- WEATHER -----*/
    theme.weather = color565(COLOR_WEATHER);
    /*----- NAMEDAY -----*/
    theme.nameday_label = color565(COLOR_NAMEDAY_LABEL);
    theme.nameday = color565(COLOR_NAMEDAY);
    /*----- CLOCK -----*/
    theme.clock = color565(COLOR_CLOCK);
    theme.clockbg = color565(COLOR_CLOCK_BG);
    theme.seconds = color565(COLOR_SECONDS);
    /*----- DATE & DAY -----*/
    theme.date = color565(COLOR_DATE);
    /*----- FOOTER -----*/
    theme.buffer = color565(COLOR_BUFFER);
    theme.ip = color565(COLOR_IP);
    theme.ip_bg = color565(COLOR_IP_BG);
    theme.ip_border = color565(COLOR_IP_BORDER);
    theme.ch = color565(COLOR_CH);
    theme.ch_bg = color565(COLOR_CH_BG);
    theme.ch_border = color565(COLOR_CH_BORDER);
    theme.rssi = color565(COLOR_RSSI);
    theme.rssi_bg = color565(COLOR_RSSI_BG);
    theme.rssi_border = color565(COLOR_RSSI_BORDER);
    /*----- BITRATE WIDGET -----*/
    theme.bitrate = color565(COLOR_BITRATE);
    /*----- VU WIDGET -----*/
    theme.vumax = color565(COLOR_VU_MAX);
    theme.vumid = color565(COLOR_VU_MID);
    theme.vumin = color565(COLOR_VU_MIN);
    theme.vulrbox = color565(COLOR_VU_LRBOX);
    theme.vulrtext = color565(COLOR_VU_LRTEXT);
    /*----- PLAYLIST WIDGET-----*/
    theme.plcurrent = color565(COLOR_PL_CURRENT);
    theme.plcurrentbg = color565(COLOR_PL_CURRENT_BG);
    theme.plcurrentfill = color565(COLOR_PL_CURRENT_FILL);
    theme.playlist[0] = color565(COLOR_PLAYLIST_0);
    theme.playlist[1] = color565(COLOR_PLAYLIST_1);
    theme.playlist[2] = color565(COLOR_PLAYLIST_2);
    theme.playlist[3] = color565(COLOR_PLAYLIST_3);
    theme.playlist[4] = color565(COLOR_PLAYLIST_4);
    /*----- PRESETS -----*/
    theme.prst_button = color565(COLOR_PRST_BUTTON);
    theme.prst_card = color565(COLOR_PRST_CARD);
    theme.prst_accent = color565(COLOR_PRST_ACCENT);
    theme.prst_fav = color565(COLOR_PRST_FAV);
    theme.prst_title1 = color565(COLOR_PRST_TITLE_1);
    theme.prst_title2 = color565(COLOR_PRST_TITLE_2);
    theme.prst_title3 = color565(COLOR_PRST_TITLE_3);
    theme.prst_line = color565(COLOR_PRST_LINE);
    /*----- VOLUME WIDGET -----*/
    theme.vol_bg = color565(COLOR_VOL_BG);
    theme.vol_border = color565(COLOR_VOL_BORDER);
    theme.vol_inactive = color565(COLOR_VOL_INACTIVE);
    theme.vol_low = color565(COLOR_VOL_LOW);
    theme.vol_mid = color565(COLOR_VOL_MID);
    theme.vol_high = color565(COLOR_VOL_HIGH);
    theme.vol_icon = color565(COLOR_VOL_ICON);
    /*----- WIFI WIDGET -----*/
    theme.wifi_bg = color565(COLOR_WIFI_BG);
    theme.wifi_border = color565(COLOR_WIFI_BORDER);
    theme.wifi_inactive = color565(COLOR_WIFI_INACTIVE);
    theme.wifi_low = color565(COLOR_WIFI_LOW);
    theme.wifi_low_mid = color565(COLOR_WIFI_LOW_MID);
    theme.wifi_mid = color565(COLOR_WIFI_MID);
    theme.wifi_high = color565(COLOR_WIFI_HIGH);
}

bool Config::setThemeColorByName(const char* name, uint8_t r, uint8_t g, uint8_t b) {
    if (!name || !*name) { return false; }
    const uint16_t c = color565(r, g, b);

#define SET_THEME_COLOR(_name, _field)   \
    if (equalsIgnoreCase(name, _name)) { \
        theme._field = c;                \
        return true;                     \
    }

    SET_THEME_COLOR("background", background);
    SET_THEME_COLOR("meta", meta);
    SET_THEME_COLOR("station_name", meta);
    SET_THEME_COLOR("metabg", metabg);
    SET_THEME_COLOR("station_bg", metabg);
    SET_THEME_COLOR("metafill", metafill);
    SET_THEME_COLOR("station_fill", metafill);
    SET_THEME_COLOR("title1", title1);
    SET_THEME_COLOR("sng_title_1", title1);
    SET_THEME_COLOR("title2", title2);
    SET_THEME_COLOR("sng_title_2", title2);
    SET_THEME_COLOR("bitrate", bitrate);
    SET_THEME_COLOR("weather", weather);
    SET_THEME_COLOR("digit", digit);
    SET_THEME_COLOR("digits", digit);
    SET_THEME_COLOR("nameday_label", nameday_label);
    SET_THEME_COLOR("nameday", nameday);
    SET_THEME_COLOR("clock", clock);
    SET_THEME_COLOR("clockbg", clockbg);
    SET_THEME_COLOR("clock_bg", clockbg);
    SET_THEME_COLOR("seconds", seconds);
    SET_THEME_COLOR("div", div);
    SET_THEME_COLOR("divider", div);
    SET_THEME_COLOR("date", date);
    SET_THEME_COLOR("vumax", vumax);
    SET_THEME_COLOR("vu_max", vumax);
    SET_THEME_COLOR("vumid", vumid);
    SET_THEME_COLOR("vu_mid", vumid);
    SET_THEME_COLOR("vumin", vumin);
    SET_THEME_COLOR("vu_min", vumin);
    SET_THEME_COLOR("vulrbox", vulrbox);
    SET_THEME_COLOR("vu_lrbox", vulrbox);
    SET_THEME_COLOR("vulrtext", vulrtext);
    SET_THEME_COLOR("vu_lrtext", vulrtext);
    SET_THEME_COLOR("vol_bg", vol_bg);
    SET_THEME_COLOR("vol_border", vol_border);
    SET_THEME_COLOR("vol_inactive", vol_inactive);
    SET_THEME_COLOR("vol_low", vol_low);
    SET_THEME_COLOR("vol_mid", vol_mid);
    SET_THEME_COLOR("vol_high", vol_high);
    SET_THEME_COLOR("vol_icon", vol_icon);
    SET_THEME_COLOR("wifi_bg", wifi_bg);
    SET_THEME_COLOR("wifi_border", wifi_border);
    SET_THEME_COLOR("wifi_inactive", wifi_inactive);
    SET_THEME_COLOR("wifi_low", wifi_low);
    SET_THEME_COLOR("wifi_low_mid", wifi_low_mid);
    SET_THEME_COLOR("wifi_mid", wifi_mid);
    SET_THEME_COLOR("wifi_high", wifi_high);
    SET_THEME_COLOR("ip_text", ip);
    SET_THEME_COLOR("ip", ip);
    SET_THEME_COLOR("ip_bg", ip_bg);
    SET_THEME_COLOR("ip_border", ip_border);
    SET_THEME_COLOR("ch_text", ch);
    SET_THEME_COLOR("ch", ch);
    SET_THEME_COLOR("ch_bg", ch_bg);
    SET_THEME_COLOR("ch_border", ch_border);
    SET_THEME_COLOR("rssi_text", rssi);
    SET_THEME_COLOR("rssi", rssi);
    SET_THEME_COLOR("rssi_bg", rssi_bg);
    SET_THEME_COLOR("rssi_border", rssi_border);
    SET_THEME_COLOR("buffer", buffer);
    SET_THEME_COLOR("pl_current", plcurrent);
    SET_THEME_COLOR("plcurrent", plcurrent);
    SET_THEME_COLOR("pl_current_bg", plcurrentbg);
    SET_THEME_COLOR("plcurrentbg", plcurrentbg);
    SET_THEME_COLOR("pl_current_fill", plcurrentfill);
    SET_THEME_COLOR("plcurrentfill", plcurrentfill);
    SET_THEME_COLOR("prst_button", prst_button);
    SET_THEME_COLOR("prst_card", prst_card);
    SET_THEME_COLOR("prst_accent", prst_accent);
    SET_THEME_COLOR("prst_fav", prst_fav);
    SET_THEME_COLOR("prst_title1", prst_title1);
    SET_THEME_COLOR("prst_title2", prst_title2);
    SET_THEME_COLOR("prst_title3", prst_title3);
    SET_THEME_COLOR("prst_line", prst_line);

#undef SET_THEME_COLOR

    if (equalsIgnoreCase(name, "playlist_0") || equalsIgnoreCase(name, "playlist0")) {
        theme.playlist[0] = c;
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_1") || equalsIgnoreCase(name, "playlist1")) {
        theme.playlist[1] = c;
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_2") || equalsIgnoreCase(name, "playlist2")) {
        theme.playlist[2] = c;
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_3") || equalsIgnoreCase(name, "playlist3")) {
        theme.playlist[3] = c;
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_4") || equalsIgnoreCase(name, "playlist4")) {
        theme.playlist[4] = c;
        return true;
    }

    return false;
}

bool Config::getThemeColorByName(const char* name, uint16_t& color) const {
    if (!name || !*name) { return false; }

#define GET_THEME_COLOR(_name, _field)   \
    if (equalsIgnoreCase(name, _name)) { \
        color = theme._field;            \
        return true;                     \
    }

    GET_THEME_COLOR("background", background);
    GET_THEME_COLOR("meta", meta);
    GET_THEME_COLOR("station_name", meta);
    GET_THEME_COLOR("metabg", metabg);
    GET_THEME_COLOR("station_bg", metabg);
    GET_THEME_COLOR("metafill", metafill);
    GET_THEME_COLOR("station_fill", metafill);
    GET_THEME_COLOR("title1", title1);
    GET_THEME_COLOR("sng_title_1", title1);
    GET_THEME_COLOR("title2", title2);
    GET_THEME_COLOR("sng_title_2", title2);
    GET_THEME_COLOR("bitrate", bitrate);
    GET_THEME_COLOR("weather", weather);
    GET_THEME_COLOR("digit", digit);
    GET_THEME_COLOR("digits", digit);
    GET_THEME_COLOR("nameday_label", nameday_label);
    GET_THEME_COLOR("nameday", nameday);
    GET_THEME_COLOR("clock", clock);
    GET_THEME_COLOR("clockbg", clockbg);
    GET_THEME_COLOR("clock_bg", clockbg);
    GET_THEME_COLOR("seconds", seconds);
    GET_THEME_COLOR("div", div);
    GET_THEME_COLOR("divider", div);
    GET_THEME_COLOR("date", date);
    GET_THEME_COLOR("vumax", vumax);
    GET_THEME_COLOR("vu_max", vumax);
    GET_THEME_COLOR("vumid", vumid);
    GET_THEME_COLOR("vu_mid", vumid);
    GET_THEME_COLOR("vumin", vumin);
    GET_THEME_COLOR("vu_min", vumin);
    GET_THEME_COLOR("vulrbox", vulrbox);
    GET_THEME_COLOR("vu_lrbox", vulrbox);
    GET_THEME_COLOR("vulrtext", vulrtext);
    GET_THEME_COLOR("vu_lrtext", vulrtext);
    GET_THEME_COLOR("vol_bg", vol_bg);
    GET_THEME_COLOR("vol_border", vol_border);
    GET_THEME_COLOR("vol_inactive", vol_inactive);
    GET_THEME_COLOR("vol_low", vol_low);
    GET_THEME_COLOR("vol_mid", vol_mid);
    GET_THEME_COLOR("vol_high", vol_high);
    GET_THEME_COLOR("vol_icon", vol_icon);
    GET_THEME_COLOR("wifi_bg", wifi_bg);
    GET_THEME_COLOR("wifi_border", wifi_border);
    GET_THEME_COLOR("wifi_inactive", wifi_inactive);
    GET_THEME_COLOR("wifi_low", wifi_low);
    GET_THEME_COLOR("wifi_low_mid", wifi_low_mid);
    GET_THEME_COLOR("wifi_mid", wifi_mid);
    GET_THEME_COLOR("wifi_high", wifi_high);
    GET_THEME_COLOR("ip_text", ip);
    GET_THEME_COLOR("ip", ip);
    GET_THEME_COLOR("ip_bg", ip_bg);
    GET_THEME_COLOR("ip_border", ip_border);
    GET_THEME_COLOR("ch_text", ch);
    GET_THEME_COLOR("ch", ch);
    GET_THEME_COLOR("ch_bg", ch_bg);
    GET_THEME_COLOR("ch_border", ch_border);
    GET_THEME_COLOR("rssi_text", rssi);
    GET_THEME_COLOR("rssi", rssi);
    GET_THEME_COLOR("rssi_bg", rssi_bg);
    GET_THEME_COLOR("rssi_border", rssi_border);
    GET_THEME_COLOR("buffer", buffer);
    GET_THEME_COLOR("pl_current", plcurrent);
    GET_THEME_COLOR("plcurrent", plcurrent);
    GET_THEME_COLOR("pl_current_bg", plcurrentbg);
    GET_THEME_COLOR("plcurrentbg", plcurrentbg);
    GET_THEME_COLOR("pl_current_fill", plcurrentfill);
    GET_THEME_COLOR("plcurrentfill", plcurrentfill);
    GET_THEME_COLOR("prst_button", prst_button);
    GET_THEME_COLOR("prst_card", prst_card);
    GET_THEME_COLOR("prst_accent", prst_accent);
    GET_THEME_COLOR("prst_fav", prst_fav);
    GET_THEME_COLOR("prst_title1", prst_title1);
    GET_THEME_COLOR("prst_title2", prst_title2);
    GET_THEME_COLOR("prst_title3", prst_title3);
    GET_THEME_COLOR("prst_line", prst_line);

#undef GET_THEME_COLOR

    if (equalsIgnoreCase(name, "playlist_0") || equalsIgnoreCase(name, "playlist0")) {
        color = theme.playlist[0];
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_1") || equalsIgnoreCase(name, "playlist1")) {
        color = theme.playlist[1];
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_2") || equalsIgnoreCase(name, "playlist2")) {
        color = theme.playlist[2];
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_3") || equalsIgnoreCase(name, "playlist3")) {
        color = theme.playlist[3];
        return true;
    }
    if (equalsIgnoreCase(name, "playlist_4") || equalsIgnoreCase(name, "playlist4")) {
        color = theme.playlist[4];
        return true;
    }

    return false;
}

bool Config::applyThemeCsv(const char* csvData) {
    if (!csvData) { return false; }
    bool        changed = false;
    char        line[160];
    const char* p = csvData;

    while (*p) {
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof(line) - 1) {
            line[n++] = *p;
            ++p;
        }
        if (*p == '\n') { ++p; }
        line[n] = '\0';
        if (n > 0 && line[n - 1] == '\r') { line[n - 1] = '\0'; }

        char* row = trimInPlace(line);
        if (*row == '\0' || *row == '#') { continue; }

        char* saveptr = nullptr;
        char* name = strtok_r(row, ",", &saveptr);
        char* rs = strtok_r(nullptr, ",", &saveptr);
        char* gs = strtok_r(nullptr, ",", &saveptr);
        char* bs = strtok_r(nullptr, ",", &saveptr);

        if (!name || !rs || !gs || !bs) { continue; }

        name = trimInPlace(name);
        rs = trimInPlace(rs);
        gs = trimInPlace(gs);
        bs = trimInPlace(bs);

        if (equalsIgnoreCase(name, "version")) { continue; }

        char* end = nullptr;
        long  rv = strtol(rs, &end, 10);
        if (end == rs) { continue; }
        long gv = strtol(gs, &end, 10);
        if (end == gs) { continue; }
        long bv = strtol(bs, &end, 10);
        if (end == bs) { continue; }

        if (setThemeColorByName(name, clampChannel(rv), clampChannel(gv), clampChannel(bv))) { changed = true; }
    }

    return changed;
}

bool Config::loadThemeFromFile(const char* path) {
    if (!path || !LittleFS.exists(path)) { return false; }
    File file = LittleFS.open(path, "r");
    if (!file) { return false; }
    String content;
    content.reserve(file.size() + 1);
    while (file.available()) {
        content += file.readStringUntil('\n');
        content += '\n';
    }
    file.close();
    return applyThemeCsv(content.c_str());
}

bool Config::saveThemeToFile() {
    if (!LittleFS.exists("/data")) { LittleFS.mkdir("/data"); }

    File file = LittleFS.open(THEME_PATH, "w");
    if (!file) { return false; }

    uint8_t v1, v2, v3;
    parseVersionTriplet(THEME_CSV_VERSION, v1, v2, v3);
    file.printf("version,%u,%u,%u\n", v1, v2, v3);

    auto writeColor = [&](const char* key, uint16_t color) {
        uint8_t r, g, b;
        color565ToRgb(color, r, g, b);
        file.printf("%s,%u,%u,%u\n", key, r, g, b);
    };

    writeColor("background", theme.background);
    writeColor("meta", theme.meta);
    writeColor("metabg", theme.metabg);
    writeColor("metafill", theme.metafill);
    writeColor("title1", theme.title1);
    writeColor("title2", theme.title2);
    writeColor("bitrate", theme.bitrate);
    writeColor("weather", theme.weather);
    writeColor("digit", theme.digit);
    writeColor("nameday_label", theme.nameday_label);
    writeColor("nameday", theme.nameday);
    writeColor("clock", theme.clock);
    writeColor("clockbg", theme.clockbg);
    writeColor("seconds", theme.seconds);
    writeColor("div", theme.div);
    writeColor("date", theme.date);
    writeColor("vumax", theme.vumax);
    writeColor("vumid", theme.vumid);
    writeColor("vumin", theme.vumin);
    writeColor("vulrbox", theme.vulrbox);
    writeColor("vulrtext", theme.vulrtext);
    writeColor("vol_bg", theme.vol_bg);
    writeColor("vol_border", theme.vol_border);
    writeColor("vol_inactive", theme.vol_inactive);
    writeColor("vol_low", theme.vol_low);
    writeColor("vol_mid", theme.vol_mid);
    writeColor("vol_high", theme.vol_high);
    writeColor("vol_icon", theme.vol_icon);
    writeColor("wifi_bg", theme.wifi_bg);
    writeColor("wifi_border", theme.wifi_border);
    writeColor("wifi_inactive", theme.wifi_inactive);
    writeColor("wifi_low", theme.wifi_low);
    writeColor("wifi_low_mid", theme.wifi_low_mid);
    writeColor("wifi_mid", theme.wifi_mid);
    writeColor("wifi_high", theme.wifi_high);
    writeColor("ip", theme.ip);
    writeColor("ip_bg", theme.ip_bg);
    writeColor("ip_border", theme.ip_border);
    writeColor("ch", theme.ch);
    writeColor("ch_bg", theme.ch_bg);
    writeColor("ch_border", theme.ch_border);
    writeColor("rssi", theme.rssi);
    writeColor("rssi_bg", theme.rssi_bg);
    writeColor("rssi_border", theme.rssi_border);
    writeColor("buffer", theme.buffer);
    writeColor("pl_current", theme.plcurrent);
    writeColor("pl_current_bg", theme.plcurrentbg);
    writeColor("pl_current_fill", theme.plcurrentfill);
    writeColor("playlist_0", theme.playlist[0]);
    writeColor("playlist_1", theme.playlist[1]);
    writeColor("playlist_2", theme.playlist[2]);
    writeColor("playlist_3", theme.playlist[3]);
    writeColor("playlist_4", theme.playlist[4]);
    writeColor("prst_button", theme.prst_button);
    writeColor("prst_card", theme.prst_card);
    writeColor("prst_accent", theme.prst_accent);
    writeColor("prst_fav", theme.prst_fav);
    writeColor("prst_title1", theme.prst_title1);
    writeColor("prst_title2", theme.prst_title2);
    writeColor("prst_title3", theme.prst_title3);
    writeColor("prst_line", theme.prst_line);

    file.close();
    return true;
}

String Config::themeToJson() const {
    String out = "{";
    bool   first = true;

    auto appendColor = [&](const char* key, uint16_t color) {
        uint8_t r, g, b;
        color565ToRgb(color, r, g, b);
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%02X%02X%02X", r, g, b);
        if (!first) { out += ","; }
        first = false;
        out += "\"";
        out += key;
        out += "\":\"";
        out += colorHex;
        out += "\"";
    };

    appendColor("background", theme.background);
    appendColor("meta", theme.meta);
    appendColor("metabg", theme.metabg);
    appendColor("metafill", theme.metafill);
    appendColor("title1", theme.title1);
    appendColor("title2", theme.title2);
    appendColor("bitrate", theme.bitrate);
    appendColor("weather", theme.weather);
    appendColor("digit", theme.digit);
    appendColor("nameday_label", theme.nameday_label);
    appendColor("nameday", theme.nameday);
    appendColor("clock", theme.clock);
    appendColor("clockbg", theme.clockbg);
    appendColor("seconds", theme.seconds);
    appendColor("div", theme.div);
    appendColor("date", theme.date);
    appendColor("vumax", theme.vumax);
    appendColor("vumid", theme.vumid);
    appendColor("vumin", theme.vumin);
    appendColor("vulrbox", theme.vulrbox);
    appendColor("vulrtext", theme.vulrtext);
    appendColor("vol_bg", theme.vol_bg);
    appendColor("vol_border", theme.vol_border);
    appendColor("vol_inactive", theme.vol_inactive);
    appendColor("vol_low", theme.vol_low);
    appendColor("vol_mid", theme.vol_mid);
    appendColor("vol_high", theme.vol_high);
    appendColor("vol_icon", theme.vol_icon);
    appendColor("wifi_bg", theme.wifi_bg);
    appendColor("wifi_border", theme.wifi_border);
    appendColor("wifi_inactive", theme.wifi_inactive);
    appendColor("wifi_low", theme.wifi_low);
    appendColor("wifi_low_mid", theme.wifi_low_mid);
    appendColor("wifi_mid", theme.wifi_mid);
    appendColor("wifi_high", theme.wifi_high);
    appendColor("ip", theme.ip);
    appendColor("ip_bg", theme.ip_bg);
    appendColor("ip_border", theme.ip_border);
    appendColor("ch", theme.ch);
    appendColor("ch_bg", theme.ch_bg);
    appendColor("ch_border", theme.ch_border);
    appendColor("rssi", theme.rssi);
    appendColor("rssi_bg", theme.rssi_bg);
    appendColor("rssi_border", theme.rssi_border);
    appendColor("buffer", theme.buffer);
    appendColor("pl_current", theme.plcurrent);
    appendColor("pl_current_bg", theme.plcurrentbg);
    appendColor("pl_current_fill", theme.plcurrentfill);
    appendColor("playlist_0", theme.playlist[0]);
    appendColor("playlist_1", theme.playlist[1]);
    appendColor("playlist_2", theme.playlist[2]);
    appendColor("playlist_3", theme.playlist[3]);
    appendColor("playlist_4", theme.playlist[4]);
    appendColor("prst_button", theme.prst_button);
    appendColor("prst_card", theme.prst_card);
    appendColor("prst_accent", theme.prst_accent);
    appendColor("prst_fav", theme.prst_fav);
    appendColor("prst_title1", theme.prst_title1);
    appendColor("prst_title2", theme.prst_title2);
    appendColor("prst_title3", theme.prst_title3);
    appendColor("prst_line", theme.prst_line);

    out += "}";
    return out;
}

void Config::reset() {
    setDefaults();
    delay(500);
    ESP.restart();
}
void Config::enableScreensaver(bool val) {
    saveValue(&store.screensaverEnabled, val);
    display.putRequest(NEWMODE, PLAYER);
}
void Config::setScreensaverTimeout(uint16_t val) {
    val = constrain(val, 5, 65520);
    saveValue(&store.screensaverTimeout, val);
    display.putRequest(NEWMODE, PLAYER);
}
void Config::setScreensaverBlank(bool val) {
    saveValue(&store.screensaverBlank, val);
    display.putRequest(NEWMODE, PLAYER);
}
void Config::setScreensaverPlayingEnabled(bool val) {
    saveValue(&store.screensaverPlayingEnabled, val);
    display.putRequest(NEWMODE, PLAYER);
}
void Config::setScreensaverPlayingTimeout(uint16_t val) {
    val = constrain(val, 1, 1080);
    config.saveValue(&config.store.screensaverPlayingTimeout, val);
    display.putRequest(NEWMODE, PLAYER);
}
void Config::setScreensaverPlayingBlank(bool val) {
    saveValue(&store.screensaverPlayingBlank, val);
    display.putRequest(NEWMODE, PLAYER);
}
void Config::setSntpOne(const char* val) {
    bool tzdone = false;
    if (strlen(val) > 0 && strlen(store.sntp2) > 0) {
        configTime(store.tzHour * 3600 + store.tzMin * 60, getTimezoneOffset(), val, store.sntp2);
        tzdone = true;
    } else if (strlen(val) > 0) {
        configTime(store.tzHour * 3600 + store.tzMin * 60, getTimezoneOffset(), val);
        tzdone = true;
    }
    if (tzdone) {
        timekeeper.forceTimeSync = true;
        saveValue(config.store.sntp1, val, 35);
    }
}
void Config::setShowweather(bool val) {
    config.saveValue(&config.store.showweather, val);
    timekeeper.forceWeather = true;
    display.putRequest(SHOWWEATHER);
}
void Config::setWeatherKey(const char* val) {
    saveValue(store.weatherkey, val, WEATHERKEY_LENGTH);
    display.putRequest(NEWMODE, CLEAR);
    display.putRequest(NEWMODE, PLAYER);
}

#if IR_PIN != 255
void Config::setIrBtn(int val) {
    irBtnId = val;
    netserver.irRecordEnable = (irBtnId >= 0);
    irBankId = 0;
    netserver.irValsToWs(); // kiküldi a három mentett gombot a webszervernek
    IRCommand ircmd;
    if (val >= 0) {
        ircmd.irBtnId = val;   // a gombhoz tartozó index, -1 a mentéséshez
        ircmd.hasBtnId = true; // mentés engedélyezése
        ircmd.irBankId = 0;    // 0, 1, 2
        ircmd.hasBank = true;  // mentés engedélyezése
        xQueueSend(irQueue, &ircmd, 0);
        Serial.printf("config.cpp--> setIrBtn--> xQueueSend\n");
    } else {
        saveIR();
        Serial.println("config.cpp--> setIrBtn--> val: -1 (save)");
    }
}
#endif

void Config::resetSystem(const char* val, uint8_t clientId) {
    BOOTLOG("***************** RESET SYSTEM *****************");
    if (strcmp(val, "system") == 0) {
        saveValue(&store.smartstart, (uint8_t)2, false);
        saveValue(&store.audioinfo, false, false);
        saveValue(&store.vumeter, false, false);
        saveValue(&store.vuPeak, true, false);
        saveValue(&store.vuPeakInitMarker, static_cast<uint8_t>(0xA5), false);
        saveValue(&store.vuBidirectional, false, false);
        saveValue(&store.softapdelay, (uint8_t)0, false);
        saveValue(&store.watchdog, true);
        saveValue(&store.stallWatchdog, true, false);
        saveValue(&store.serialLittlefsEnabled, true, false);
        saveValue(&store.nameday, true);
        saveValue(&store.clockTtsEnabled, false, false);
        saveValue(store.clockTtsLanguage, "HU", sizeof(store.clockTtsLanguage), false);
        saveValue(&store.clockTtsIntervalMinutes, static_cast<uint16_t>(15));
        saveValue(&store.clockTtsOnlyWhenNoStream, false, false);
        saveValue(&store.clockTtsQuietHoursEnabled, false, false);
        saveValue(&store.clockTtsQuietFromMinutes, static_cast<uint16_t>(23 * 60), false);
        saveValue(&store.clockTtsQuietToMinutes, static_cast<uint16_t>(7 * 60));
        saveValue(&store.clockFontStyle, static_cast<uint8_t>(CLOCKFONT_STYLE));
        saveValue(&store.clockFontMono, static_cast<bool>(CLOCKFONT_MONO_DEFAULT));
        saveValue(&store.clockAmPmStyle, static_cast<bool>(CLOCK_AM_PM_STYLE_DEFAULT));
        snprintf(store.mdnsname, MDNS_LENGTH, "VTom_Radio-%x", (unsigned int)getChipId());
        saveValue(store.mdnsname, store.mdnsname, MDNS_LENGTH, true, true);
        display.putRequest(NEWMODE, CLEAR);
        display.putRequest(NEWMODE, PLAYER);
        netserver.requestOnChange(GETSYSTEM, clientId);
        return;
    }
    if (strcmp(val, "screen") == 0) {
        saveValue(&store.flipscreen, false, false);
        display.flip();
        saveValue(&store.invertdisplay, false, false);
        display.invert();
        display.flip();
        saveValue(&store.dspon, true, false);
        store.brightness = 100;
        setBrightness(false);
        saveValue(&store.contrast, (uint8_t)55, false);
        display.setContrast();
        saveValue(&store.numplaylist, false);
        saveValue(&store.playlistMovingCursor, false);
        saveValue(&store.directChannelChange, false);
        saveValue(&store.stationsListReturnTime, (uint8_t)3);
        saveValue(&store.screensaverEnabled, false);
        saveValue(&store.screensaverTimeout, (uint16_t)20);
        saveValue(&store.screensaverBlank, false);
        saveValue(&store.screensaverPlayingEnabled, false);
        saveValue(&store.screensaverPlayingTimeout, (uint16_t)5);
        saveValue(&store.screensaverPlayingBlank, false);
        saveValue(&store.fadeEnabled, (uint8_t)FADE_ENABLED, true);
        saveValue(&store.fadeStartDelay, (uint16_t)FADE_START_DELAY, true);
        saveValue(&store.fadeTarget, (uint8_t)FADE_TARGET, true);
        saveValue(&store.fadeStep, (uint8_t)FADE_STEP, true);
        display.putRequest(NEWMODE, CLEAR);
        display.putRequest(NEWMODE, PLAYER);
        netserver.requestOnChange(GETSCREEN, clientId);
        return;
    }
    if (strcmp(val, "timezone") == 0) {
        saveValue(&store.tzHour, (int8_t)3, false);
        saveValue(&store.tzMin, (int8_t)0, false);
        saveValue(store.sntp1, "hu.pool.ntp.org", 35, false);
        saveValue(store.sntp2, "time.google.com", 35);
        saveValue(&store.timeSyncInterval, (uint16_t)60);
        saveValue(&store.timeSyncIntervalRTC, (uint16_t)24);
        configTime(store.tzHour * 3600 + store.tzMin * 60, getTimezoneOffset(), store.sntp1, store.sntp2);
        timekeeper.forceTimeSync = true;
        netserver.requestOnChange(GETTIMEZONE, clientId);
        return;
    }
    if (strcmp(val, "weather") == 0) {
        saveValue(&store.showweather, false, false);
        saveValue(store.weatherlat, "46.3873", 10, false);
        saveValue(store.weatherlon, "18.1513", 10, false);
        saveValue(store.weatherkey, "", WEATHERKEY_LENGTH);
        saveValue(&store.weatherSyncInterval, (uint16_t)30);
        // network.trueWeather=false;
        display.putRequest(NEWMODE, CLEAR);
        display.putRequest(NEWMODE, PLAYER);
        netserver.requestOnChange(GETWEATHER, clientId);
        return;
    }
    if (strcmp(val, "controls") == 0) {
        saveValue(&store.fliptouch, false, false);
        saveValue(&store.dbgtouch, false, false);
#if TS_MODEL == TS_MODEL_FT6X36
        saveValue(&store.xTouchMirroring, false, false);
        saveValue(&store.yTouchMirroring, false, false);
#else
        saveValue(&store.xTouchMirroring, true, false);
        saveValue(&store.yTouchMirroring, true, false);
#endif
        saveValue(&store.skipPlaylistUpDown, false);
        saveValue(&store.encodersIndependent, false);
        setIRTolerance(40);
        netserver.requestOnChange(GETCONTROLS, clientId);
        return;
    }
    if (strcmp(val, "1") == 0) {
        config.reset();
        return;
    }
}

void Config::setDefaults() {
    BOOTLOG("***************** SET DEFAULT *****************");
    store.config_set = 0;
    store.version = CONFIG_VERSION;
    store.volume = 6;
    store.balance = 0;
    store.trebble = 0;
    store.middle = 0;
    store.bass = 0;
    store.lastStation = 0;
    store.countStation = 0;
    store.lastSSID = 0;
    store.audioinfo = false;
    store.smartstart = 2;
    store.tzHour = 2;
    store.tzMin = 0;
    store.timezoneOffset = 0;
    store.vumeter = false;
    store.vuBidirectional = false;
    store.softapdelay = 0;
    store.flipscreen = false;
    store.invertdisplay = false;
    store.numplaylist = false;
    store.fliptouch = false;
    store.dbgtouch = false;
    store.dspon = true;
    store.brightness = 100;
    store.contrast = 55;
    strlcpy(store.sntp1, "hu.pool.ntp.org", 35);
    strlcpy(store.sntp2, "time.google.com", 35);
    store.showweather = false;
    strlcpy(store.weatherlat, "46.3873", 10);
    strlcpy(store.weatherlon, "18.1513", 10);
    strlcpy(store.weatherkey, "", WEATHERKEY_LENGTH);
    store.vuPeak = true;
    store.vuPeakInitMarker = 0xA5;
    store.lastSdStation = 0;
    store.lastDlnaStation = 0; // DLNA mod
    store.sdsnuffle = false;
    store.play_mode = 0;
    store.irtlp = 35;
    store.btnpressticks = 500;
    store.screensaverEnabled = false;
    store.screensaverTimeout = 20;
    store.screensaverBlank = false;
    snprintf(store.mdnsname, MDNS_LENGTH, "radio-%x", (unsigned int)getChipId());
    store.skipPlaylistUpDown = false;
    store.encodersIndependent = false;
    store.screensaverPlayingEnabled = false;
    store.screensaverPlayingTimeout = 5;
    store.screensaverPlayingBlank = false;
    // store.abuff = VS1053_CS == 255 ? 7 : 10;
    store.watchdog = true;
    store.nameday = true;
    store.clockTtsEnabled = false;
    strlcpy(store.clockTtsLanguage, "HU", sizeof(store.clockTtsLanguage));
    store.clockTtsIntervalMinutes = 15;
    store.clockTtsOnlyWhenNoStream = false;
    store.clockTtsQuietHoursEnabled = false;
    store.clockTtsQuietFromMinutes = 23 * 60;
    store.clockTtsQuietToMinutes = 7 * 60;
    store.clockFontStyle = CLOCKFONT_STYLE;
    store.clockFontMono = CLOCKFONT_MONO_DEFAULT;
    store.clockAmPmStyle = CLOCK_AM_PM_STYLE_DEFAULT;
    store.timeSyncInterval = 60;    // min
    store.timeSyncIntervalRTC = 24; // hour
    store.weatherSyncInterval = 30; // min
    store.fadeEnabled = FADE_ENABLED;
    store.fadeStartDelay = FADE_START_DELAY;
    store.fadeTarget = FADE_TARGET;
    store.fadeStep = FADE_STEP;
    store.playlistSource = PL_SRC_WEB;
    store.dateFormat = 0;
    store.playlistMovingCursor = false;
    store.directChannelChange = false;
    store.stationsListReturnTime = 3;
    store.stallWatchdog = true;
    store.serialLittlefsEnabled = true;
#if TS_MODEL == TS_MODEL_FT6X36
    store.xTouchMirroring = false;
    store.yTouchMirroring = false;
#else
    store.xTouchMirroring = true;
    store.yTouchMirroring = false;
#endif

    store.rssiAsText = false;
    eepromWrite(EEPROM_START, store);
}

void Config::setTimezone(int8_t tzh, int8_t tzm) {
    saveValue(&store.tzHour, tzh, false);
    saveValue(&store.tzMin, tzm);
}

void Config::setTimezoneOffset(uint16_t tzo) {
    saveValue(&store.timezoneOffset, tzo);
}

uint16_t Config::getTimezoneOffset() {
    return 0; // TODO
}
// Véletlen lejátszás beállítása.
void Config::setSnuffle(bool sn) {
    saveValue(&store.sdsnuffle, sn);
    // if(store.sdsnuffle) player.next(); //Továbbléptette egy másik fájlra, ezért kivettem.
}

#if IR_PIN != 255
void Config::saveIR() {
    eepromWrite(EEPROM_START_IR, ircodes);
    Serial.println("IR codes saved to EEPROM");
}
#endif

void Config::saveVolume() {
    saveValue(&store.volume, store.volume, true, true);
}

uint8_t Config::setVolume(uint8_t val) {
    store.volume = val;
    display.putRequest(DRAWVOL);
    netserver.requestOnChange(VOLUME, 0);
    return store.volume;
}

void Config::setTone(int8_t bass, int8_t middle, int8_t trebble) {
    saveValue(&store.bass, bass, false);
    saveValue(&store.middle, middle, false);
    saveValue(&store.trebble, trebble);
    player.setTone(store.bass, store.middle, store.trebble);
    netserver.requestOnChange(EQUALIZER, 0);
}

void Config::setSmartStart(uint8_t ss) {
    saveValue(&store.smartstart, ss);
}

void Config::setVuBidirectional(bool val) {
    saveValue(&store.vuBidirectional, val);
}

void Config::setBalance(int8_t balance) {
    saveValue(&store.balance, balance);
    player.setBalance(-store.balance); // "audio_change"  -16 to 16 fordítás 16 to -16
    netserver.requestOnChange(BALANCE, 0);
}

uint8_t Config::setLastStation(uint16_t val) {
    // Make "current item" persistent per mode
    if (getMode() == PM_SDCARD) {
        saveValue(&store.lastSdStation, val);
        return store.lastSdStation;
    }
#ifdef USE_DLNA
    if (store.playlistSource == PL_SRC_DLNA) {
        saveValue(&store.lastDlnaStation, val);
        return store.lastDlnaStation;
    }
#endif
    saveValue(&store.lastStation, val);
    return store.lastStation;
}

uint8_t Config::setCountStation(uint16_t val) {
    saveValue(&store.countStation, val);
    return store.countStation;
}

uint8_t Config::setLastSSID(uint8_t val) {
    saveValue(&store.lastSSID, val);
    return store.lastSSID;
}

void Config::setTitle(const char* title) {
    vuRefLevel = 0;
    memset(config.station.title, 0, BUFLEN);
    strlcpy(config.station.title, title, BUFLEN);
    u8fix(config.station.title);
    netserver.requestOnChange(TITLE, 0);
    netserver.loop();
    display.putRequest(NEWTITLE);
}

void Config::setStation(const char* station) {
    memset(config.station.name, 0, BUFLEN);
    strlcpy(config.station.name, station, BUFLEN);
    u8fix(config.station.title);
}

void Config::indexPlaylist() {
    File playlist = LittleFS.open(PLAYLIST_PATH, "r");
    if (!playlist) { return; }
    int  sOvol;
    File index = LittleFS.open(INDEX_PATH, "w");
    while (playlist.available()) {
        uint32_t pos = playlist.position();
        if (parseCSV(playlist.readStringUntil('\n').c_str(), tmpBuf, tmpBuf2, sOvol)) { index.write((uint8_t*)&pos, 4); }
    }
    index.close();
    playlist.close();
}

// DLNA mod
#ifdef USE_DLNA
void Config::indexDLNAPlaylist() {
    File playlist = LittleFS.open(PLAYLIST_DLNA_PATH, "r");
    if (!playlist) {
        Serial.println("[DLNA][IDX] Cannot open DLNA playlist");
        return;
    }

    File index = LittleFS.open(INDEX_DLNA_PATH, "w");
    if (!index) {
        Serial.println("[DLNA][IDX] Cannot create DLNA index");
        playlist.close();
        return;
    }

    static char lineBuf[512];
    int         sOvol = 0;

    uint32_t lines = 0;
    uint32_t ok = 0;

    while (playlist.available()) {
        uint32_t pos = playlist.position();

        // readBytesUntil nem allokál, stabil
        size_t n = playlist.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[n] = 0;

        // CRLF kezelés
        if (n > 0 && lineBuf[n - 1] == '\r') { lineBuf[n - 1] = 0; }

        // üres sor skip
        if (lineBuf[0] == 0) {
            lines++;
            continue;
        }

        // FONTOS: parseCSV kapjon ÍRHATÓ buffert (lineBuf), ne String.c_str()-t
        if (parseCSV(lineBuf, tmpBuf, tmpBuf2, sOvol)) {
            index.write((uint8_t*)&pos, 4);
            ok++;
        }

        lines++;

        // WDT/Task starvation ellen (DLNA/WiFi közben kellhet)
        if ((lines % 50) == 0) {
            delay(0); // vagy yield();
        }
    }

    index.close();
    playlist.close();

    Serial.printf("[DLNA][IDX] DLNA playlist indexed: %lu/%lu\n", (unsigned long)ok, (unsigned long)lines);
}
#endif

void Config::initPlaylist() {
    // store.countStation = 0;
    if (!LittleFS.exists(INDEX_PATH)) { indexPlaylist(); }

    /*if (LittleFS.exists(INDEX_PATH)) {
      File index = LittleFS.open(INDEX_PATH, "r");
      store.countStation = index.size() / 4;
      index.close();
      saveValue(&store.countStation, store.countStation, true, true);
    }*/
}

#ifdef USE_DLNA // DLNA mod
void Config::initDLNAPlaylist() {
    indexDLNAPlaylist();

    if (LittleFS.exists(INDEX_DLNA_PATH)) {
        File index = LittleFS.open(INDEX_DLNA_PATH, "r");
        if (index) {
            // lastStation(_randomStation());
            index.close();
        }
    }
}
#endif

uint16_t Config::playlistLength() {
    uint16_t out = 0;
    if (SDPLFS()->exists(REAL_INDEX)) {
        File index = SDPLFS()->open(REAL_INDEX, "r");
        out = index.size() / 4;
        index.close();
    }
    return out;
}
bool Config::loadStation(uint16_t ls) {
    int      sOvol;
    uint16_t cs = playlistLength();
    if (cs == 0) {
        memset(station.url, 0, BUFLEN);
        memset(station.name, 0, BUFLEN);
        strncpy(station.name, "yoRadio", BUFLEN);
        station.ovol = 0;
        return false;
    }
    if (ls > playlistLength()) { ls = 1; }
    File playlist = SDPLFS()->open(REAL_PLAYL, "r");
    File index = SDPLFS()->open(REAL_INDEX, "r");
    index.seek((ls - 1) * 4, SeekSet);
    uint32_t pos;
    index.readBytes((char*)&pos, 4);
    index.close();
    playlist.seek(pos, SeekSet);
    if (parseCSV(playlist.readStringUntil('\n').c_str(), tmpBuf, tmpBuf2, sOvol)) {
        memset(station.url, 0, BUFLEN);
        memset(station.name, 0, BUFLEN);
        strncpy(station.name, tmpBuf, BUFLEN);
        strncpy(station.url, tmpBuf2, BUFLEN);
        station.ovol = sOvol;
        setLastStation(ls);
    }
    playlist.close();
    return true;
}

char* Config::stationByNum(uint16_t num) {
    File playlist = SDPLFS()->open(REAL_PLAYL, "r");
    File index = SDPLFS()->open(REAL_INDEX, "r");
    index.seek((num - 1) * 4, SeekSet);
    uint32_t pos;
    memset(_stationBuf, 0, sizeof(_stationBuf));
    index.readBytes((char*)&pos, 4);
    index.close();
    playlist.seek(pos, SeekSet);
    strncpy(_stationBuf, playlist.readStringUntil('\t').c_str(), sizeof(_stationBuf));
    playlist.close();
    return _stationBuf;
}

void Config::escapeQuotes(const char* input, char* output, size_t maxLen) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < maxLen - 1; ++i) {
        if (input[i] == '"' && j < maxLen - 2) {
            output[j++] = '\\';
            output[j++] = '"';
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

bool Config::parseCSV(const char* line, char* name, char* url, int& ovol) {
    char*       tmpe;
    const char* cursor = line;
    char        buf[5];
    tmpe = strstr(cursor, "\t");
    if (tmpe == NULL) { return false; }
    strlcpy(name, cursor, tmpe - cursor + 1);
    if (strlen(name) == 0) { return false; }
    cursor = tmpe + 1;
    tmpe = strstr(cursor, "\t");
    if (tmpe == NULL) { return false; }
    strlcpy(url, cursor, tmpe - cursor + 1);
    if (strlen(url) == 0) { return false; }
    cursor = tmpe + 1;
    if (strlen(cursor) == 0) { return false; }
    strlcpy(buf, cursor, 4);
    ovol = atoi(buf);
    return true;
}

bool Config::parseJSON(const char* line, char* name, char* url, int& ovol) {
    char *      tmps, *tmpe;
    const char* cursor = line;
    char        port[8], host[246], file[254];
    tmps = strstr(cursor, "\":\"");
    if (tmps == NULL) { return false; }
    tmpe = strstr(tmps, "\",\"");
    if (tmpe == NULL) { return false; }
    strlcpy(name, tmps + 3, tmpe - tmps - 3 + 1);
    if (strlen(name) == 0) { return false; }
    cursor = tmpe + 3;
    tmps = strstr(cursor, "\":\"");
    if (tmps == NULL) { return false; }
    tmpe = strstr(tmps, "\",\"");
    if (tmpe == NULL) { return false; }
    strlcpy(host, tmps + 3, tmpe - tmps - 3 + 1);
    if (strlen(host) == 0) { return false; }
    if (strstr(host, "http://") == NULL && strstr(host, "https://") == NULL) {
        sprintf(file, "http://%s", host);
        strlcpy(host, file, strlen(file) + 1);
    }
    cursor = tmpe + 3;
    tmps = strstr(cursor, "\":\"");
    if (tmps == NULL) { return false; }
    tmpe = strstr(tmps, "\",\"");
    if (tmpe == NULL) { return false; }
    strlcpy(file, tmps + 3, tmpe - tmps - 3 + 1);
    cursor = tmpe + 3;
    tmps = strstr(cursor, "\":\"");
    if (tmps == NULL) { return false; }
    tmpe = strstr(tmps, "\",\"");
    if (tmpe == NULL) { return false; }
    strlcpy(port, tmps + 3, tmpe - tmps - 3 + 1);
    int p = atoi(port);
    if (p > 0) {
        sprintf(url, "%s:%d%s", host, p, file);
    } else {
        sprintf(url, "%s%s", host, file);
    }
    cursor = tmpe + 3;
    tmps = strstr(cursor, "\":\"");
    if (tmps == NULL) { return false; }
    tmpe = strstr(tmps, "\"}");
    if (tmpe == NULL) { return false; }
    strlcpy(port, tmps + 3, tmpe - tmps - 3 + 1);
    ovol = atoi(port);
    return true;
}

bool Config::parseWsCommand(const char* line, char* cmd, char* val, uint8_t cSize) {
    char* tmpe;
    tmpe = strstr(line, "=");
    if (tmpe == NULL) { return false; }
    memset(cmd, 0, cSize);
    strlcpy(cmd, line, tmpe - line + 1);
    // if (strlen(tmpe + 1) == 0) return false;
    memset(val, 0, cSize);
    strlcpy(val, tmpe + 1, strlen(line) - strlen(cmd) + 1);
    return true;
}

bool Config::parseSsid(const char* line, char* ssid, char* pass) {
    char* tmpe;
    tmpe = strstr(line, "\t");
    if (tmpe == NULL) { return false; }
    uint16_t pos = tmpe - line;
    if (pos > 29 || strlen(line) > 71) { return false; }
    memset(ssid, 0, 30);
    strlcpy(ssid, line, pos + 1);
    memset(pass, 0, 40);
    strlcpy(pass, line + pos + 1, strlen(line) - pos);
    return true;
}

bool Config::saveWifiFromNextion(const char* post) {
    if (!LittleFS.exists("/data")) { LittleFS.mkdir("/data"); }
    File file = LittleFS.open(SSIDS_PATH, "w");
    if (!file) {
        Serial.printf("[WIFI] saveWifiFromNextion: cannot open %s for write\n", SSIDS_PATH);
        return false;
    } else {
        file.print(post);
        file.close();
        ESP.restart();
        return true;
    }
}

bool Config::saveWifi() {
    if (!LittleFS.exists("/data")) { LittleFS.mkdir("/data"); }
    if (!LittleFS.exists(TMP_PATH)) { return false; }
    LittleFS.remove(SSIDS_PATH);
    LittleFS.rename(TMP_PATH, SSIDS_PATH);
    ESP.restart();
    return true;
}

void Config::setTimeConf() {
    if (strlen(store.sntp1) > 0 && strlen(store.sntp2) > 0) {
        configTime(store.tzHour * 3600 + store.tzMin * 60, getTimezoneOffset(), store.sntp1, store.sntp2);
    } else if (strlen(store.sntp1) > 0) {
        configTime(store.tzHour * 3600 + store.tzMin * 60, getTimezoneOffset(), store.sntp1);
    }
}

bool Config::initNetwork() {
    File file = LittleFS.open(SSIDS_PATH, "r");
    if (!file || file.isDirectory()) { return false; }
    char    ssidval[30], passval[40];
    uint8_t c = 0;
    while (file.available()) {
        if (parseSsid(file.readStringUntil('\n').c_str(), ssidval, passval)) {
            strlcpy(ssids[c].ssid, ssidval, 30);
            strlcpy(ssids[c].password, passval, 40);
            ssidsCount++;
            c++;
        }
    }
    file.close();
    return true;
}

void Config::setBrightness(bool dosave) {
    if (!store.dspon && dosave) { display.wakeup(); }
    display.setBrightnessPercent(store.brightness);
    if (!store.dspon) { store.dspon = true; }
    if (dosave) {
        saveValue(&store.brightness, store.brightness, false, true);
        saveValue(&store.dspon, store.dspon, true, true);
    }
#ifdef USE_NEXTION
    nextion.wake();
    char cmd[15];
    snprintf(cmd, 15, "dims=%d", store.brightness);
    nextion.putcmd(cmd);
    if (!store.dspon) { store.dspon = true; }
    if (dosave) {
        saveValue(&store.brightness, store.brightness, false, true);
        saveValue(&store.dspon, store.dspon, true, true);
    }
#endif
}

void Config::setDspOn(bool dspon, bool saveval) {
    if (saveval) {
        store.dspon = dspon;
        saveValue(&store.dspon, store.dspon, true, true);
    }
#ifdef USE_NEXTION
    if (!dspon) {
        nextion.sleep();
    } else {
        nextion.wake();
    }
#endif
    if (!dspon) {
#if BRIGHTNESS_PIN != 255
        analogWrite(BRIGHTNESS_PIN, 0);
#endif
        display.deepsleep();
    } else {
        display.wakeup();
#if BRIGHTNESS_PIN != 255
        analogWrite(BRIGHTNESS_PIN, map(store.brightness, 0, 100, 0, 255));
#endif
    }
}

void Config::doSleep() {
    if (BRIGHTNESS_PIN != 255) { analogWrite(BRIGHTNESS_PIN, 0); }
    display.deepsleep();
#ifdef USE_NEXTION
    nextion.sleep();
#endif
    uint64_t mask = 0;
#if WAKE_PIN1 >= 0 && WAKE_PIN1 < 64
    if (rtc_gpio_is_valid_gpio((gpio_num_t)WAKE_PIN1)) {
        rtc_gpio_pullup_en((gpio_num_t)WAKE_PIN1);
        rtc_gpio_pulldown_dis((gpio_num_t)WAKE_PIN1);
        mask |= (1ULL << WAKE_PIN1);
    }
#endif
#if WAKE_PIN2 >= 0 && WAKE_PIN2 < 64
    if (rtc_gpio_is_valid_gpio((gpio_num_t)WAKE_PIN2)) {
        rtc_gpio_pullup_en((gpio_num_t)WAKE_PIN2);
        rtc_gpio_pulldown_dis((gpio_num_t)WAKE_PIN2);
        mask |= (1ULL << WAKE_PIN2);
    }
#endif
    if (mask != 0) { esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW); }
    esp_sleep_enable_timer_wakeup(config.sleepfor * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void Config::doSleepW() {
    analogWrite(BRIGHTNESS_PIN, 0);           // ← add (MB)
    pinMode(BRIGHTNESS_PIN, OUTPUT);          // ← add (MB)
    digitalWrite(BRIGHTNESS_PIN, LOW);        // ← add (MB)
    gpio_hold_en((gpio_num_t)BRIGHTNESS_PIN); // ← add (MB)
    gpio_deep_sleep_hold_en();                // ← add (MB)
    display.deepsleep();
    
#ifdef USE_NEXTION
        nextion.sleep();
#endif
    uint64_t mask = 0;
#if WAKE_PIN1 >= 0 && WAKE_PIN1 < 64
    if (rtc_gpio_is_valid_gpio((gpio_num_t)WAKE_PIN1)) {
        rtc_gpio_pullup_en((gpio_num_t)WAKE_PIN1);
        rtc_gpio_pulldown_dis((gpio_num_t)WAKE_PIN1);
        mask |= (1ULL << WAKE_PIN1);
    }
#endif
#if WAKE_PIN2 >= 0 && WAKE_PIN2 < 64
    if (rtc_gpio_is_valid_gpio((gpio_num_t)WAKE_PIN2)) {
        rtc_gpio_pullup_en((gpio_num_t)WAKE_PIN2);
        rtc_gpio_pulldown_dis((gpio_num_t)WAKE_PIN2);
        mask |= (1ULL << WAKE_PIN2);
    }
#endif
    delay(200);
    if (mask != 0) { esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW); }
    esp_deep_sleep_start();
}

void Config::sleepForAfter(uint16_t sf, uint16_t sa) {
    sleepfor = sf;
    if (sa > 0) {
        timekeeper.waitAndDo(sa * 60, doSleep);
    } else {
        doSleep();
    }
}

/*----- number to formated string -----*/
const char* fmtThousands(uint32_t v) {
    static char buf[16];
    char        tmp[16];
    sprintf(tmp, "%lu", v);

    int len = strlen(tmp);
    int pos = len % 3;
    int j = 0;

    for (int i = 0; i < len; i++) {
        if (i && (i % 3) == pos) buf[j++] = ' ';
        buf[j++] = tmp[i];
    }
    buf[j] = 0;

    return buf;
}

void Config::bootInfo() {
    BOOTLOG("************************************************");
    BOOTLOG("*            VTom Radio v%s                    *", FW_VERSION);
    BOOTLOG("************************************************");
    BOOTLOG("------------------------------------------------");
    BOOTLOG("arduino:\t%d", ARDUINO);
    BOOTLOG("compiler:\t%s", __VERSION__);
    BOOTLOG("esp32core:\t%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8) { chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i; }
    BOOTLOG("chip:\t\tmodel: %s | rev: %d | id: %lu | cores: %d | psram: %lu", ESP.getChipModel(), ESP.getChipRevision(), chipId, ESP.getChipCores(), ESP.getPsramSize());
    BOOTLOG("display:\t%d", DSP_MODEL);
    BOOTLOG("audio:\t\t%s (%d, %d, %d, mclk:%d)", "I2S", I2S_DOUT, I2S_BCLK, I2S_LRC, I2S_MCLK);
    BOOTLOG("audioinfo:\t%s", store.audioinfo ? "true" : "false");
    BOOTLOG("smartstart:\t%d", store.smartstart);
    BOOTLOG("vumeter:\t%s", store.vumeter ? "true" : "false");
    BOOTLOG("softapdelay:\t%d", store.softapdelay);
    BOOTLOG("flipscreen:\t%s", store.flipscreen ? "true" : "false");
    BOOTLOG("invertdisplay:\t%s", store.invertdisplay ? "true" : "false");
    BOOTLOG("showweather:\t%s", store.showweather ? "true" : "false");
    BOOTLOG("buttons:\tleft=%d, center=%d, right=%d, up=%d, down=%d, mode=%d, pullup=%s", BTN_LEFT, BTN_CENTER, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_MODE, BTN_INTERNALPULLUP ? "true" : "false");
    BOOTLOG("encoders:\tl1=%d, b1=%d, r1=%d, pullup=%s, l2=%d, b2=%d, r2=%d, pullup=%s", ENC_BTNL, ENC_BTNB, ENC_BTNR, ENC_INTERNALPULLUP ? "true" : "false", ENC2_BTNL, ENC2_BTNB, ENC2_BTNR,
            ENC2_INTERNALPULLUP ? "true" : "false");
    BOOTLOG("ir:\t\t%d", IR_PIN);
    if (SDC_CS != 255) { BOOTLOG("SD:\t\t%d", SDC_CS); }

    BOOTLOG("------------------------------------------------");
    BOOTLOG("CONFIG:\tsizeof(store)=%u B | EEPROM_START=%u | EEPROM_END=%u | EEPROM_SIZE=%u", (unsigned)sizeof(config.store), (unsigned)EEPROM_START, (unsigned)(EEPROM_START + sizeof(config.store)),
            (unsigned)EEPROM_SIZE);
    BOOTLOG("------------------------------------------------");
    BOOTLOG("------------- EEPROM AFTER READ ----------------");
    BOOTLOG("fadeEnabled   : %s", store.fadeEnabled ? "true" : "false");
    BOOTLOG("fadeStartDelay: %4s", fmtThousands(store.fadeStartDelay));
    BOOTLOG("fadeTarget    : %4s", fmtThousands(store.fadeTarget));
    BOOTLOG("fadeStep      : %4s", fmtThousands(store.fadeStep));
    BOOTLOG("Serial LittleFS : %s", store.serialLittlefsEnabled ? "true" : "false");
    BOOTLOG("------------------------------------------------");
    BOOTLOG("----------------- HEAP AND PSRAM ---------------");
    BOOTLOG("Total heap : %10s byte", fmtThousands(ESP.getHeapSize()));
    BOOTLOG("Free heap  : %10s byte", fmtThousands(ESP.getFreeHeap()));
    BOOTLOG(psramFound() ? "✅ PSRAM found!" : "❌ PSRAM not found!");
    BOOTLOG("Total PSRAM: %10s byte", fmtThousands(ESP.getPsramSize()));
    BOOTLOG("Free PSRAM : %10s byte", fmtThousands(ESP.getFreePsram()));
    BOOTLOG("------------------------------------------------");
}
