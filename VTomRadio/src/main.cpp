#include "Arduino.h"
#include "core/clock_tts.h" // "clock_tts"
#include "core/options.h"
#include "core/config.h"
#include "pluginsManager/pluginsManager.h"
#include "plugins/backlight/backlight.h" // backlight plugin
#include "core/player.h"
#include "core/display.h"
#include "core/network.h"
#include "core/netserver.h"
#include "core/controls.h"
// #include "core/mqtt.h"
#include "driver/rtc_io.h"
#include "core/serial_littlefs.h"

#include "core/optionschecker.h"
#include "core/timekeeper.h"
#ifdef USE_NEXTION
#    include "displays/nextion.h"
#endif
#include "core/audiohandlers.h" //"audio_change"
#ifdef USE_DLNA                 // DLNA mod
#    include "network/dlna_service.h"
#endif
#if USE_OTA
#    if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#        include <NetworkUdp.h>
#    else
#        include <WiFiUdp.h>
#    endif
#    include <ArduinoOTA.h>
#endif

#if DSP_HSPI || TS_HSPI || VS_HSPI
SPIClass SPI2(HSPI);
#endif

extern __attribute__((weak)) void radio_on_setup();

#if USE_OTA
void setupOTA() {
    if (strlen(config.store.mdnsname) > 0) ArduinoOTA.setHostname(config.store.mdnsname);
#    ifdef OTA_PASS
    ArduinoOTA.setPassword(OTA_PASS);
#    endif
    ArduinoOTA
        .onStart([]() {
            player.sendCommand({PR_STOP, 0});
            display.putRequest(NEWMODE, UPDATING);
            Serial.printf("Start OTA updating %s\r\n", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
        })
        .onEnd([]() {
            Serial.printf("\nEnd OTA update, Rebooting...\r\n");
            ESP.restart();
        })
        .onProgress([](unsigned int progress, unsigned int total) { Serial.printf("Progress OTA: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {
                Serial.printf("Auth Failed\r\n");
            } else if (error == OTA_BEGIN_ERROR) {
                Serial.printf("Begin Failed\r\n");
            } else if (error == OTA_CONNECT_ERROR) {
                Serial.printf("Connect Failed\r\n");
            } else if (error == OTA_RECEIVE_ERROR) {
                Serial.printf("Receive Failed\r\n");
            } else if (error == OTA_END_ERROR) {
                Serial.printf("End Failed\r\n");
            }
        });
    ArduinoOTA.begin();
}
#endif

#include "IRremoteESP8266/IRrecv.h"
#include "IRremoteESP8266/IRutils.h"

extern IRrecv         irrecv;
extern decode_results irResults;
// void checkMaintenanceMode();

static TaskHandle_t clockTtsTaskHandle = nullptr;

static void clockTtsTask(void* /*param*/) {
    while (true) {
        clock_tts_loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void startClockTtsTask() {
    if (clockTtsTaskHandle != nullptr) return;
#if CONFIG_FREERTOS_UNICORE
    constexpr BaseType_t targetCore = 0;
#else
    constexpr BaseType_t targetCore = 1;
#endif
    xTaskCreatePinnedToCore(clockTtsTask, "clock_tts", 4096, nullptr, 1, &clockTtsTaskHandle, targetCore);
}

//#define MAINTENANCE_PIN 39 // pl. GPIO0 (boot gomb is lehet)
bool maintenanceMode = false;

static bool serviceMaintenanceMode() {
    static bool maintenanceScreenShown = false;
    serial_littlefs_poll();
    if (!serial_littlefs_is_active()) {
        maintenanceScreenShown = false;
        return false;
    }
    if (!maintenanceScreenShown) {
        display_show_maintenance_screen();
        maintenanceScreenShown = true;
    }
    delay(2);
    return true;
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);

#if IR_PIN != 255
    irQueue = xQueueCreate(4, sizeof(IRCommand));
    config.eepromRead(EEPROM_START_IR, config.ircodes);
    irWakeup();
#endif

serial_littlefs_begin(Serial);   //Botfai Tibor: sor hozzáadva a serial_littlefs kezeléséhez.

// Boot window: ESP 4 másodpercig várja a maintenance kapcsolatot.
// A Python HELLO → BEGIN kézfogással lép be; END paranccsal vagy idle timeout
// után a maintenance mód le is tud zárni, és a normál boot folytatódik.
{
    uint32_t bootEnd = millis() + 4000;
    while (millis() < bootEnd) {
        serial_littlefs_poll();
        if (serial_littlefs_is_active()) {
            display_show_maintenance_screen();
            while (serial_littlefs_is_active()) {
                serial_littlefs_poll();
                delay(1);
            }
            break;  // END parancs vagy idle timeout után folytatja a normál bootot
        }
        delay(5);
    }
}

#if (BRIGHTNESS_PIN != 255) // backlight plugin
    Serial.printf("Exists? %p\n", &backlightPlugin);
    backlightPluginInit();
#endif
    if (REAL_LEDBUILTIN != 255) pinMode(REAL_LEDBUILTIN, OUTPUT);
    if (radio_on_setup) radio_on_setup();
    pm.init();     // pluginsManager
    pm.on_setup(); // pluginsManager
    config.init();
    display.init();
    player.init();
    network.begin();
    if (network.status != CONNECTED && network.status != SDREADY) {
        netserver.begin();
        initControls();
        display.putRequest(DSP_START);
        while (!display.ready()) {
            if (serviceMaintenanceMode()) continue;
            delay(10);
        }
        return;
    }
    if (SDC_CS != 255) {
        display.putRequest(WAITFORSD, 0);
        Serial.print("##[BOOT]#\tSD search\t");
    }
    config.initPlaylistMode();
    netserver.begin();
    initControls();
    display.putRequest(DSP_START);
    while (!display.ready()) {
        if (serviceMaintenanceMode()) continue;
        delay(10);
    }
#if USE_OTA
    setupOTA();
#endif
    if (config.getMode() == PM_SDCARD) player.initHeaders(config.station.url);
    player.lockOutput = false;
    if (config.store.smartstart == 1) { player.sendCommand({PR_PLAY, config.lastStation()}); }
    clock_tts_setup();
    startClockTtsTask();
    Audio::audio_info_callback = my_audio_info; // "audio_change" audiohandlers.h ban kezelve.
    pinMode(POWER_LED, OUTPUT);
    if (POWER_LED != 255) { digitalWrite(POWER_LED, HIGH); }
    pm.on_end_setup();
}

void loop() {
    if (serviceMaintenanceMode()) return;

    timekeeper.loop1();
    if (network.status == CONNECTED || network.status == SDREADY) {
        player.loop();
#if USE_OTA
        ArduinoOTA.handle();
#endif
    }
    loopControls();
#ifdef NETSERVER_LOOP1
    netserver.loop();
#endif
}
