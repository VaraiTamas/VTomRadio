// v0.9.720
#include "options.h"
#include <ESPmDNS.h>
#include "time.h"
#include "rtcsupport.h"
#include "network.h"
#include "display.h"
#include "config.h"
#include "netserver.h"
#include "player.h"
#include "mqtt.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"

#ifndef WIFI_ATTEMPTS
    #define WIFI_ATTEMPTS 16
#endif

#ifndef SEARCH_WIFI_CORE_ID
    #define SEARCH_WIFI_CORE_ID 0
#endif
MyNetwork network;
static portMUX_TYPE networkTimeMux = portMUX_INITIALIZER_UNLOCKED;

void network_get_timeinfo_snapshot(struct tm* out) {
    if (!out) return;
    portENTER_CRITICAL(&networkTimeMux);
    *out = network.timeinfo;
    portEXIT_CRITICAL(&networkTimeMux);
}

void network_set_timeinfo(const struct tm& in) {
    portENTER_CRITICAL(&networkTimeMux);
    network.timeinfo = in;
    portEXIT_CRITICAL(&networkTimeMux);
}

void MyNetwork::WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    network.beginReconnect = false;
    player.lockOutput = false;
    delay(100);
    display.putRequest(NEWMODE, PLAYER);
    if (config.getMode() == PM_SDCARD) {
        network.status = CONNECTED;
        display.putRequest(NEWIP, 0);
    } else {
        display.putRequest(NEWMODE, PLAYER);
        if (network.lostPlaying) player.sendCommand({PR_PLAY, config.lastStation()});
    }
#ifdef MQTT_ROOT_TOPIC
    connectToMqtt();
#endif
}

void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!network.beginReconnect) {
        Serial.printf("Lost connection, reconnecting to %s...\n", config.ssids[config.store.lastSSID - 1].ssid);
        if (config.getMode() == PM_SDCARD) {
            network.status = SDREADY;
            display.putRequest(NEWIP, 0);
        } else {
            network.lostPlaying = player.isRunning();
            if (network.lostPlaying) {
                player.lockOutput = true;
                player.sendCommand({PR_STOP, 0});
            }
            display.putRequest(NEWMODE, LOST);
        }
    }
    network.beginReconnect = true;
    WiFi.reconnect();
}

bool MyNetwork::wifiBegin(bool silent) {
    // Hard Reset WiFi before starting
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000); 
    WiFi.mode(WIFI_STA);
    delay(500);
    uint8_t ls = (config.store.lastSSID == 0 || config.store.lastSSID > config.ssidsCount) ? 0 : config.store.lastSSID - 1;
    uint8_t startedls = ls;
    uint8_t errcnt = 0;
    while (true) {
        if (!silent) {
            Serial.printf("##[BOOT]#\tAttempt to connect to %s\n", config.ssids[ls].ssid);
            Serial.print("##[BOOT]#\t");
            display.putRequest(BOOTSTRING, ls);
        }
        WiFi.disconnect(true);
        uint32_t t0 = millis();
        while (WiFi.status() == WL_CONNECTED) {
            delay(100);
            if (millis() - t0 > 2000) { // 2 sec timeout
                Serial.println("WiFi disconnect timeout!");
                break;
            }
        }
        // Check if WiFi is ready
        int waitCount = 0;
        while (WiFi.status() == WL_DISCONNECTED && waitCount < 20) {
            delay(100);
            waitCount++;
        }
        // Force HIDDEN state if still connected
        if (WiFi.status() != WL_IDLE_STATUS && WiFi.status() != WL_DISCONNECTED) {
            WiFi.mode(WIFI_OFF);
            delay(500);
            WiFi.mode(WIFI_STA);
            delay(500);
        }
        WiFi.begin(config.ssids[ls].ssid, config.ssids[ls].password);
        while (WiFi.status() != WL_CONNECTED) {
            if (!silent) Serial.print(".");
            delay(500);
            if (REAL_LEDBUILTIN != 255 && !silent) digitalWrite(REAL_LEDBUILTIN, !digitalRead(REAL_LEDBUILTIN));
            errcnt++;
            if (errcnt > WIFI_ATTEMPTS) {
                errcnt = 0;
                ls++;
                if (ls > config.ssidsCount - 1) ls = 0;
                if (!silent) Serial.println();
                WiFi.mode(WIFI_OFF);
                break;
            }
        }
        if (WiFi.status() != WL_CONNECTED && ls == startedls) {
            return false;
            break;
        }
        if (WiFi.status() == WL_CONNECTED) {
            config.setLastSSID(ls + 1);
            return true;
            break;
        }
    }
    return false;
}

void searchWiFi(void* pvParameters) {
    if (!network.wifiBegin(true)) {
        delay(10000);
        xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 3, NULL, SEARCH_WIFI_CORE_ID); // "task_prioritas" 0 eredeti, új 3
    } else {
        network.status = CONNECTED;
        netserver.begin(true);
        network.setWifiParams();
        display.putRequest(NEWIP, 0);
#ifdef MQTT_ROOT_TOPIC
        mqttInit();
#endif
    }
    vTaskDelete(NULL);
}

#define DBGAP false

void MyNetwork::begin() {
    BOOTLOG("network.begin");
    config.initNetwork();
    if (config.ssidsCount == 0 || DBGAP) {
        raiseSoftAP();
        return;
    }
    if (config.getMode() != PM_SDCARD) {
        if (!wifiBegin()) {
            raiseSoftAP();
            Serial.println("##[BOOT]#\tdone");
            return;
        }
        Serial.println(".");
        status = CONNECTED;
        setWifiParams();
#ifdef MQTT_ROOT_TOPIC
        mqttInit();
#endif
    } else {
        status = SDREADY;
        xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 3, NULL, SEARCH_WIFI_CORE_ID); // "task_prioritas" 0 eredeti, 3 új
    }

    Serial.println("##[BOOT]#\tdone");
    if (REAL_LEDBUILTIN != 255) digitalWrite(REAL_LEDBUILTIN, LOW);

#if RTCSUPPORTED
    if (config.isRTCFound()) {
        tm currentTime{};
        rtc.getTime(&currentTime);
        mktime(&currentTime);
        network_set_timeinfo(currentTime);
        display.putRequest(CLOCK);
    }
#endif
    if (network_on_connect) network_on_connect();
    pm.on_connect();
}

void MyNetwork::setWifiParams() {
    WiFi.setSleep(false);
    WiFi.onEvent(WiFiReconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiLostConnection, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    // config.setTimeConf(); //??
    if (strlen(config.store.mdnsname) > 0) MDNS.begin(config.store.mdnsname);
    Serial.printf("##[BOOT]#\tWeb UI: http://%s/\n", WiFi.localIP().toString().c_str());
}

void MyNetwork::requestTimeSync() {
}

void rebootTime() {
    ESP.restart();
}

void MyNetwork::raiseSoftAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);
    Serial.println("##[BOOT]#");
    BOOTLOG("************************************************");
    BOOTLOG("Running in AP mode");
    BOOTLOG("Connect to AP %s with password %s", apSsid, apPassword);
    BOOTLOG("and go to http://192.168.4.1/ to configure");
    Serial.println("##[BOOT]#\tWeb UI (AP): http://192.168.4.1/");
    BOOTLOG("************************************************");
    status = SOFT_AP;
    if (config.store.softapdelay > 0) timekeeper.waitAndDo(config.store.softapdelay * 60, rebootTime);
}

void MyNetwork::requestWeatherSync() {
    display.putRequest(NEWWEATHER);
}