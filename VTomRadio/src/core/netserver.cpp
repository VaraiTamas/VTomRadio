// clang-format off
#include "options.h"
#include "Arduino.h"
#include <LittleFS.h>
#include <Update.h>
#include "config.h"
#include "netserver.h"
#include "player.h"
#include "display.h"
#include "network.h"
#include "mqtt.h"
#include "controls.h"
#include "commandhandler.h"
#include "timekeeper.h"
#include "../displays/widgets/widgetsconfig.h"

#ifdef USE_DLNA  //DLNA mod
  #include "../dlna/dlna_index.h"
  #include "../dlna/dlna_service.h"
#endif

#if DSP_MODEL == DSP_DUMMY
  #define DUMMYDISPLAY
#endif

#ifdef USE_SD
  #include "sdmanager.h"
#endif
#ifndef MIN_MALLOC
  #define MIN_MALLOC 24112
#endif
#ifndef NSQ_SEND_DELAY
  //#define NSQ_SEND_DELAY       portMAX_DELAY
  #define NSQ_SEND_DELAY pdMS_TO_TICKS(300)
#endif
#ifndef NS_QUEUE_TICKS
  //#define NS_QUEUE_TICKS pdMS_TO_TICKS(2)
  #define NS_QUEUE_TICKS 0
#endif

#ifdef DEBUG_V
  #define DBGVB(...)             \
    {                            \
      char buf[200];             \
      sprintf(buf, __VA_ARGS__); \
      Serial.print("[DEBUG]\t"); \
      Serial.println(buf);       \
    }
#else
  #define DBGVB(...)
#endif

//#define CORS_DEBUG //Enable CORS policy: 'Access-Control-Allow-Origin' (for testing)

NetServer netserver;

AsyncWebServer webserver(80);
AsyncWebSocket websocket("/ws");

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleIndex(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

bool shouldReboot = false;

static String   g_wifiScanCache = "[]";
static uint32_t g_wifiScanCacheMs = 0;
static bool     g_wifiScanRunning = false;
static uint32_t g_wifiScanStartMs = 0;
static constexpr uint32_t WIFI_SCAN_CACHE_TTL_MS = 15000;
static constexpr uint32_t WIFI_SCAN_MAX_WAIT_MS = 15000;
static bool     g_webboardUploadHadError = false;

static bool isEmptyFsKnownPath(const String &url) {
  return url == "/" || url == "/webboard" || url == "/variables.js" || url == "/wifiscan" || url == "/favicon.ico" || url == "/emergency";
}

static bool isCaptivePortalProbePath(const String &url) {
  return url == "/generate_204" || url == "/gen_204" || url == "/hotspot-detect.html" || url == "/library/test/success.html"
         || url == "/connecttest.txt" || url == "/ncsi.txt" || url == "/success.txt" || url == "/fwlink";
}

static bool shouldRedirectEmptyFsRequest(AsyncWebServerRequest *request) {
  if (!config.emptyFS || request->method() != HTTP_GET) {
    return false;
  }

  const String &url = request->url();
  if (isEmptyFsKnownPath(url)) {
    return false;
  }

  if (isCaptivePortalProbePath(url)) {
    return true;
  }

  const String &host = request->host();
  if (!host.length()) {
    return false;
  }

  const String apHost = WiFi.softAPIP().toString();
  return host != apHost && host != (apHost + ":80");
}

static bool hasRequiredWebboardFiles() {
  const char *requiredWebFiles[] = {
    "dragpl.js", "ir.css", "irrecord.html", "ir.js", "logo.svg", "options.html", "player.html", "script.js", "style.css", "updform.html", "theme.css", "theme-editor.html"
  };
  const char *requiredFonts[] = {
    "roboto9.vlw", "roboto12.vlw", "roboto16.vlw", "roboto18.vlw", "roboto20.vlw", "roboto22.vlw", "roboto24.vlw", "roboto26.vlw", "roboto36.vlw"
  };

  bool ok = true;
  for (size_t i = 0; i < sizeof(requiredWebFiles) / sizeof(requiredWebFiles[0]); i++) {
    String plainPath = String("/www/") + requiredWebFiles[i];
    String gzPath = plainPath + ".gz";
    if (!LittleFS.exists(plainPath) && !LittleFS.exists(gzPath)) {
      Serial.printf("[WEBBOARD] Missing required www file: %s\n", requiredWebFiles[i]);
      ok = false;
    }
  }

  for (size_t i = 0; i < sizeof(requiredFonts) / sizeof(requiredFonts[0]); i++) {
    String fontPath = String("/fonts/") + requiredFonts[i];
    if (!LittleFS.exists(fontPath)) {
      Serial.printf("[WEBBOARD] Missing required font file: %s\n", requiredFonts[i]);
      ok = false;
    }
  }

  return ok;
}

static void finalizeWifiScanCache(int n) {
  struct ScanItem {
    String ssid;
    int32_t rssi;
  };

  auto jsonEscape = [](String s) {
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    return s;
  };

  if (n <= 0) {
    g_wifiScanCache = "[]";
    g_wifiScanCacheMs = millis();
    WiFi.scanDelete();
    return;
  }

  const int maxItems = 40;
  if (n > maxItems) {
    n = maxItems;
  }

  ScanItem items[maxItems];
  int count = 0;

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) {
      continue;
    }

    int existing = -1;
    for (int j = 0; j < count; j++) {
      if (items[j].ssid == ssid) {
        existing = j;
        break;
      }
    }

    int32_t rssi = WiFi.RSSI(i);
    if (existing >= 0) {
      if (rssi > items[existing].rssi) {
        items[existing].rssi = rssi;
      }
    } else {
      items[count].ssid = ssid;
      items[count].rssi = rssi;
      count++;
    }
  }

  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (items[j].rssi > items[i].rssi) {
        ScanItem tmp = items[i];
        items[i] = items[j];
        items[j] = tmp;
      }
    }
  }

  String out = "[";
  for (int i = 0; i < count; i++) {
    if (i) {
      out += ",";
    }
    out += "{\"ssid\":\"" + jsonEscape(items[i].ssid) + "\",\"rssi\":" + String(items[i].rssi) + "}";
  }
  out += "]";

  g_wifiScanCache = out;
  g_wifiScanCacheMs = millis();
  WiFi.scanDelete();
}

static void updateWifiScanCache() {
  if (!g_wifiScanRunning) {
    return;
  }

  int scanState = WiFi.scanComplete();

  if (scanState == WIFI_SCAN_RUNNING) {
    if (millis() - g_wifiScanStartMs > WIFI_SCAN_MAX_WAIT_MS) {
      WiFi.scanDelete();
      g_wifiScanRunning = false;
      g_wifiScanStartMs = 0;
    }
    return;
  }

  g_wifiScanRunning = false;
  g_wifiScanStartMs = 0;

  if (scanState == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    return;
  }

  finalizeWifiScanCache(scanState);
}

static void startWifiScanIfNeeded() {
  updateWifiScanCache();
  if (g_wifiScanRunning) {
    return;
  }

  bool cacheFresh = g_wifiScanCacheMs != 0 && (millis() - g_wifiScanCacheMs < WIFI_SCAN_CACHE_TTL_MS);
  if (cacheFresh) {
    return;
  }

  int rc = WiFi.scanNetworks(true, true);
  if (rc == WIFI_SCAN_RUNNING) {
    g_wifiScanRunning = true;
    g_wifiScanStartMs = millis();
  } else if (rc >= 0) {
    finalizeWifiScanCache(rc);
  }
}
#ifdef MQTT_ROOT_TOPIC
//Ticker mqttplaylistticker;
bool mqttplaylistblock = false;
void mqttplaylistSend() {
  mqttplaylistblock = true;
  //  mqttplaylistticker.detach();
  mqttPublishPlaylist();
  mqttplaylistblock = false;
}
#endif

char *updateError() {
  sprintf(netserver.nsBuf, "Update failed with error (%d)<br /> %s", (int)Update.getError(), Update.errorString());
  return netserver.nsBuf;
}

bool NetServer::begin(bool quiet) {
  if (network.status == SDREADY) {
    return true;
  }
  if (!quiet) {
    Serial.print("##[BOOT]#\tnetserver.begin\t");
  }
  importRequest = IMDONE;
  irRecordEnable = false;
  playerBufMax = psramInit() ? 300000 : 1600 * config.store.abuff;
  nsQueue = xQueueCreate(20, sizeof(nsRequestParams_t));
  while (nsQueue == NULL) {
    ;
  }

  webserver.on("/", HTTP_ANY, handleIndex);
  webserver.onNotFound(handleNotFound);
  webserver.onFileUpload(handleUpload);
//DLNA mod
#ifdef USE_DLNA
  extern String g_dlnaControlUrl;

  /* ================= DLNA INIT ================= */
  webserver.on("/dlna/init", HTTP_GET, [](AsyncWebServerRequest *request) {
    //DLNA modplus

    if (dlna_isBusy()) {
      request->send(429, "application/json", "{\"queued\":false,\"busy\":true}");
      return;
    }

    config.resumeAfterModeChange = player.isRunning();
    if (config.resumeAfterModeChange) {
      player.sendCommand({PR_STOP, 0});
    }
    //DLNA modplus
    DlnaJob j{};
    j.type = DJ_INIT;
    j.reqId = dlna_next_reqId();

    dlna_worker_enqueue(j);

    request->send(202, "application/json", "{\"queued\":true}");
  });

  /* ================= DLNA LIST ================= */
  webserver.on("/dlna/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("objectId")) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing objectId\"}");
      return;
    }

    if (!g_dlnaControlUrl.length()) {
      request->send(503, "application/json", "{\"ok\":false,\"error\":\"DLNA not initialized\"}");
      return;
    }

    String objectId = request->getParam("objectId")->value();
    uint32_t start = request->hasParam("start") ? request->getParam("start")->value().toInt() : 0;

    String json;

    DlnaIndex idx;
    bool ok = idx.listContainer(g_dlnaControlUrl, objectId, json, start);

    if (!ok) {
      request->send(500, "application/json", "{\"ok\":false,\"error\":\"Browse failed\"}");
      return;
    }

    AsyncWebServerResponse *r = request->beginResponse(200, "application/json", json);
    r->addHeader("Cache-Control", "no-store");
    request->send(r);
  });

  /* ================= DLNA BUILD ================= */
  webserver.on("/dlna/build", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("objectId")) {
      request->send(400, "text/plain", "Missing objectId");
      return;
    }

    String oid = request->getParam("objectId")->value();
    Serial.printf("[DLNA][HTTP] %s objectId='%s'\n", "/dlna/build", oid.c_str());

    if (dlna_isBusy()) {
      request->send(429, "application/json", "{\"queued\":false,\"busy\":true}");
      return;
    }

    DlnaJob j{};
    j.type = DJ_BUILD;
    strlcpy(j.objectId, request->getParam("objectId")->value().c_str(), sizeof(j.objectId));
    j.reqId = dlna_next_reqId();
    j.hardLimit = request->hasParam("limit") ? request->getParam("limit")->value().toInt() : 20000;

    dlna_worker_enqueue(j);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"queued\":true,\"reqId\":%u}", (unsigned)j.reqId);
    request->send(202, "application/json", "{\"queued\":true}");
  });

  /* ================= DLNA APPEND ================= */
  webserver.on("/dlna/append", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("objectId")) {
      request->send(400, "text/plain", "Missing objectId");
      return;
    }

    String oid = request->getParam("objectId")->value();
    Serial.printf("[DLNA][HTTP] %s objectId='%s'\n", "/dlna/append", oid.c_str());

    if (dlna_isBusy()) {
      request->send(429, "application/json", "{\"queued\":false,\"busy\":true}");
      return;
    }

    DlnaJob j{};
    j.type = DJ_APPEND;
    strlcpy(j.objectId, request->getParam("objectId")->value().c_str(), sizeof(j.objectId));
    j.reqId = dlna_next_reqId();
    j.hardLimit = request->hasParam("limit") ? request->getParam("limit")->value().toInt() : 20000;

    dlna_worker_enqueue(j);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"queued\":true,\"reqId\":%u}", (unsigned)j.reqId);
    request->send(202, "application/json", "{\"queued\":true}");
  });

  /* ================= DLNA STATUS ================= */
  webserver.on("/dlna/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    //DLNA modplus
    // Build után egyszer reseteljük a DLNA indexet 1-re, hogy a lista első eleme legyen aktív.
    static uint32_t s_appliedBuildVer = 0;
    if (!g_dlnaStatus.busy && g_dlnaStatus.ok && g_dlnaStatus.playlistVer != 0 && g_dlnaStatus.playlistVer != s_appliedBuildVer
        && strstr(g_dlnaStatus.msg, "build ok") != nullptr) {

      s_appliedBuildVer = g_dlnaStatus.playlistVer;

      Serial.println("[DLNA] Build completed → reset index to 1");
    }

    char buf[256];
    snprintf(
      buf, sizeof(buf), "{\"busy\":%s,\"ok\":%s,\"err\":%d,\"reqId\":%u,\"playlistVer\":%u,\"msg\":\"%s\"}", g_dlnaStatus.busy ? "true" : "false",
      g_dlnaStatus.ok ? "true" : "false", g_dlnaStatus.err, (unsigned)g_dlnaStatus.reqId, (unsigned)g_dlnaStatus.playlistVer, g_dlnaStatus.msg
    );

    // Cache OFF a statusra
    AsyncWebServerResponse *r = request->beginResponse(200, "application/json", buf);
    r->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    r->addHeader("Pragma", "no-cache");
    request->send(r);
  });

  webserver.on("/playlist/dlna", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool resume = config.resumeAfterModeChange;

    config.store.playlistSource = PL_SRC_DLNA;
    config.saveValue(&config.store.playlistSource, (uint8_t)PL_SRC_DLNA);

  #ifdef USE_SD
    if (config.getMode() == PM_SDCARD) {
      config.changeMode(PM_WEB);
    }
  #endif

    if (config.getMode() != PM_WEB) {
      config.changeMode(PM_WEB);
    } else {
      config.loadStation(config.store.lastDlnaStation);

      if (player_on_station_change)
        player_on_station_change();
      netserver.requestOnChange(GETINDEX, 0);
    }

    if (resume) {
      Serial.println("[DLNA] Resume playback with DLNA playlist");
      player.sendCommand({PR_PLAY, (int)config.store.lastDlnaStation});
    }

    config.resumeAfterModeChange = false;

    netserver.requestOnChange(GETINDEX, 0);
    netserver.requestOnChange(GETPLAYERMODE, 0);

    request->send(200, "text/plain", "OK");
  });

  webserver.on("/playlist/web", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool resume = config.resumeAfterModeChange;
    config.resumeAfterModeChange = player.isRunning();
    Serial.printf("[MODE] WEB enter, resume=%d\n", config.resumeAfterModeChange);

    config.store.playlistSource = PL_SRC_WEB;
    config.saveValue(&config.store.playlistSource, (uint8_t)PL_SRC_WEB);

    if (config.getMode() != PM_WEB) {
      config.changeMode(PM_WEB);
    } else {
      // nincs mode reset → csak visszatöltjük az indexet
      config.loadStation(config.lastStation());

      if (player_on_station_change) {
        player_on_station_change();
      }
      netserver.requestOnChange(GETINDEX, 0);
    }

    if (resume) {
      Serial.println("[DLNA] Resume playback after browser exit");
      player.sendCommand({PR_PLAY, config.lastStation()});
    }
    config.resumeAfterModeChange = false;

    netserver.requestOnChange(GETINDEX, 0);
    netserver.requestOnChange(GETPLAYERMODE, 0);

    request->send(200, "text/plain", "OK");
  });

#endif

  auto sendWebFile = [](AsyncWebServerRequest *request, const char *plainPath, const char *gzipPath, const char *contentType) {
    if (LittleFS.exists(plainPath)) {
      request->send(LittleFS, plainPath, contentType);
      return;
    }
    if (LittleFS.exists(gzipPath)) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, gzipPath, contentType);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    request->send(404, "text/plain", "Not found");
  };

  // Explicit handlers for critical UI assets to avoid stalled static responses on some builds.
  webserver.on("/script.js", HTTP_GET, [sendWebFile](AsyncWebServerRequest *request) {
    sendWebFile(request, "/www/script.js", "/www/script.js.gz", "application/javascript");
  });
  webserver.on("/style.css", HTTP_GET, [sendWebFile](AsyncWebServerRequest *request) {
    sendWebFile(request, "/www/style.css", "/www/style.css.gz", "text/css");
  });
  webserver.on("/theme.css", HTTP_GET, [sendWebFile](AsyncWebServerRequest *request) {
    sendWebFile(request, "/www/theme.css", "/www/theme.css.gz", "text/css");
  });
  webserver.on("/dragpl.js", HTTP_GET, [sendWebFile](AsyncWebServerRequest *request) {
    sendWebFile(request, "/www/dragpl.js", "/www/dragpl.js.gz", "application/javascript");
  });

  webserver.on("/theme", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", config.themeToJson());
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    request->send(response);
  });

  webserver.on(
    "/theme", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!request->_tempFile) {
        request->send(400, "application/json", "{\"error\":\"theme body missing\"}");
        return;
      }

      request->_tempFile.close();

      File file = LittleFS.open(TMP_PATH, "r");
      if (!file) {
        request->send(500, "application/json", "{\"error\":\"cannot read uploaded theme\"}");
        return;
      }

      String csv;
      csv.reserve(file.size() + 1);
      while (file.available()) {
        csv += file.readStringUntil('\n');
        csv += '\n';
      }
      file.close();
      LittleFS.remove(TMP_PATH);

      theme_t backup = config.theme;
      config.loadTheme();
      if (!config.applyThemeCsv(csv.c_str())) {
        config.theme = backup;
        request->send(400, "application/json", "{\"error\":\"invalid theme csv\"}");
        return;
      }

      if (!config.saveThemeToFile()) {
        config.theme = backup;
        request->send(500, "application/json", "{\"error\":\"cannot persist theme\"}");
        return;
      }

      display.invalidateThemeWidgets();
      display.putRequest(NEWMODE, CLEAR);
      display.putRequest(NEWMODE, PLAYER);

      request->send(200, "application/json", config.themeToJson());
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        if (LittleFS.exists(TMP_PATH)) {
          LittleFS.remove(TMP_PATH);
        }
        request->_tempFile = LittleFS.open(TMP_PATH, "w");
      }

      if (request->_tempFile && len > 0) {
        request->_tempFile.write(data, len);
      }

      if (index + len == total && request->_tempFile) {
        request->_tempFile.flush();
      }
    }
  );

  webserver.on(
    "/theme", HTTP_PUT,
    [](AsyncWebServerRequest *request) {
      if (!request->_tempFile) {
        request->send(400, "application/json", "{\"error\":\"theme body missing\"}");
        return;
      }

      request->_tempFile.close();

      File file = LittleFS.open(TMP_PATH, "r");
      if (!file) {
        request->send(500, "application/json", "{\"error\":\"cannot read uploaded theme\"}");
        return;
      }

      String csv;
      csv.reserve(file.size() + 1);
      while (file.available()) {
        csv += file.readStringUntil('\n');
        csv += '\n';
      }
      file.close();
      LittleFS.remove(TMP_PATH);

      theme_t backup = config.theme;
      config.loadTheme();
      if (!config.applyThemeCsv(csv.c_str())) {
        config.theme = backup;
        request->send(400, "application/json", "{\"error\":\"invalid theme csv\"}");
        return;
      }

      displayMode_e currentMode = display.mode();
      if (currentMode == CLEAR) { currentMode = PLAYER; }

      display.invalidateThemeWidgets();

      display.putRequest(NEWMODE, CLEAR);
      display.putRequest(NEWMODE, currentMode);

      request->send(200, "application/json", config.themeToJson());
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        if (LittleFS.exists(TMP_PATH)) {
          LittleFS.remove(TMP_PATH);
        }
        request->_tempFile = LittleFS.open(TMP_PATH, "w");
      }

      if (request->_tempFile && len > 0) {
        request->_tempFile.write(data, len);
      }

      if (index + len == total && request->_tempFile) {
        request->_tempFile.flush();
      }
    }
  );

  webserver.on("/theme", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    config.loadTheme();
    LittleFS.remove(THEME_PATH);
    display.putRequest(NEWMODE, CLEAR);
    display.putRequest(NEWMODE, PLAYER);
    request->send(200, "application/json", config.themeToJson());
  });

  webserver.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
    response->addHeader("Cache-Control", "max-age=31536000");
    request->send(response);
  });
  webserver.on("/update.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
    response->addHeader("Cache-Control", "max-age=31536000");
    request->send(response);
  });
  webserver.on("/ir.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
    response->addHeader("Cache-Control", "max-age=31536000");
    request->send(response);
  });

  webserver.serveStatic("/", LittleFS, "/www/");
  webserver.begin();

  //if(strlen(config.store.mdnsname)>0)
  //  MDNS.begin(config.store.mdnsname);
  websocket.onEvent(onWsEvent);
  webserver.addHandler(&websocket);
  websocket.enable(true);            // mód
//websocket.setPingInterval(5000);     // 5 mp-enként ping
//websocket.setPongTimeout(3000);      // 3 mp-en belül válasz kell
#ifdef USE_DLNA  //DLNA mod
  dlna_worker_start();
#endif
  if (!quiet) {
    Serial.println("done");
  }
  return true;
}

size_t NetServer::chunkedHtmlPageCallback(uint8_t *buffer, size_t maxLen, size_t index) {
  File requiredfile;
  bool sdpl = strcmp(netserver.chunkedPathBuffer, PLAYLIST_SD_PATH) == 0;
  if (sdpl) {
    requiredfile = config.SDPLFS()->open(netserver.chunkedPathBuffer, "r");
  } else {
    requiredfile = LittleFS.open(netserver.chunkedPathBuffer, "r");
  }
  if (!requiredfile) {
    return 0;
  }
  size_t filesize = requiredfile.size();
  size_t needread = filesize - index;
  if (!needread) {
    requiredfile.close();
    display.unlock();
    return 0;
  }
#ifdef MAX_PL_READ_BYTES
  if (maxLen > MAX_PL_READ_BYTES) {
    maxLen = MAX_PL_READ_BYTES;
  }
#endif
  size_t canread = (needread > maxLen) ? maxLen : needread;
  DBGVB("[%s] seek to %d in %s and read %d bytes with maxLen=%d", __func__, index, netserver.chunkedPathBuffer, canread, maxLen);
  //netserver.loop();
  requiredfile.seek(index, SeekSet);
  requiredfile.read(buffer, canread);
  index += canread;
  if (requiredfile) {
    requiredfile.close();
  }
  return canread;
}

void NetServer::chunkedHtmlPage(const String &contentType, AsyncWebServerRequest *request, const char *path) {
  memset(chunkedPathBuffer, 0, sizeof(chunkedPathBuffer));
  strlcpy(chunkedPathBuffer, path, sizeof(chunkedPathBuffer) - 1);
  AsyncWebServerResponse *response;
#ifndef NETSERVER_LOOP1
  display.lock();
#endif
  response = request->beginChunkedResponse(contentType, chunkedHtmlPageCallback);
  response->addHeader("Cache-Control", "max-age=31536000");
  request->send(response);
}

#ifndef DSP_NOT_FLIPPED
  #define DSP_CAN_FLIPPED true
#else
  #define DSP_CAN_FLIPPED false
#endif
#if !defined(HIDE_WEATHER) && (!defined(DUMMYDISPLAY) && !defined(USE_NEXTION))
  #define SHOW_WEATHER true
#else
  #define SHOW_WEATHER false
#endif

const char *getFormat(BitrateFormat _format) {
  switch (_format) {
    case BF_MP3:  return "MP3";
    case BF_AAC:  return "AAC";
    case BF_FLAC: return "FLC";
    case BF_OGG:  return "OGG";
    case BF_WAV:  return "WAV";
    case BF_VOR:  return "VOR";
    case BF_OPU:  return "OPU";
    default:      return "bitrate";
  }
}

void NetServer::processQueue() {
  if (nsQueue == NULL) {
    return;
  }
  nsRequestParams_t request;
  if (xQueueReceive(nsQueue, &request, NS_QUEUE_TICKS)) {
    uint8_t clientId = request.clientId;
    wsBuf[0] = '\0';
    switch (request.type) {
      case PLAYLIST: getPlaylist(clientId); break;
      case PLAYLISTSAVED:
      {
#ifdef USE_SD
        if (config.getMode() == PM_SDCARD) {
          //config.indexSDPlaylist();
          config.initSDPlaylist();
        }
#endif
#ifdef USE_DLNA  //DLNA mod
        if (config.getMode() == PM_WEB && config.store.playlistSource == PL_SRC_DLNA) {
          config.indexDLNAPlaylist();
          config.initDLNAPlaylist();
          break;
        }
#endif
        if (config.getMode() == PM_WEB) {
          config.indexPlaylist();
          config.initPlaylist();
        }
        getPlaylist(clientId);
        break;
      }
      case GETACTIVE:
      {
        bool dbgact = false, nxtn = false;
        //String act = F("\"group_wifi\",");
        nsBuf[0] = '\0';
        APPEND_GROUP("group_wifi");
        if (network.status == CONNECTED) {
          //act += F("\"group_system\",");
          APPEND_GROUP("group_system");
          if (BRIGHTNESS_PIN != 255 || DSP_CAN_FLIPPED || dbgact) {
            APPEND_GROUP("group_display");
          }
#ifdef USE_NEXTION
          APPEND_GROUP("group_nextion");
          if (!SHOW_WEATHER || dbgact) {
            APPEND_GROUP("group_weather");
          }
          nxtn = true;
#endif
#if defined(LCD_I2C) || defined(DSP_OLED)
          APPEND_GROUP("group_oled");
#endif
#if !defined(HIDE_VU) && !defined(DUMMYDISPLAY)
          APPEND_GROUP("group_vu");
#endif
          if (BRIGHTNESS_PIN != 255 || nxtn || dbgact) {
            APPEND_GROUP("group_brightness");
          }
          if (DSP_CAN_FLIPPED || dbgact) {
            APPEND_GROUP("group_tft");
          }
          if (TS_MODEL != TS_MODEL_UNDEFINED || dbgact) {
            APPEND_GROUP("group_touch");
          }
          APPEND_GROUP("group_timezone");
          if (SHOW_WEATHER || dbgact) {
            APPEND_GROUP("group_weather");
          }
          APPEND_GROUP("group_controls");
          if (ENC_BTNL != 255 || ENC2_BTNL != 255 || dbgact) {
            APPEND_GROUP("group_encoder");
          }
          if (IR_PIN != 255 || dbgact) {
            APPEND_GROUP("group_ir");
          }
#if RTCSUPPORTED
          APPEND_GROUP("group_rtc");
#else
          APPEND_GROUP("group_wortc");
#endif
        }
        size_t len = strlen(nsBuf);
        if (len > 0 && nsBuf[len - 1] == ',') {
          nsBuf[len - 1] = '\0';
        }

        snprintf(wsBuf, sizeof(wsBuf), "{\"act\":[%s]}", nsBuf);
        break;
      }
      case GETINDEX:
      {
        requestOnChange(STATION, clientId);
        requestOnChange(TITLE, clientId);
        requestOnChange(VOLUME, clientId);
        requestOnChange(EQUALIZER, clientId);
        requestOnChange(BALANCE, clientId);
        requestOnChange(BITRATE, clientId);
        requestOnChange(MODE, clientId);
        requestOnChange(SDINIT, clientId);
        requestOnChange(GETPLAYERMODE, clientId);
        if (config.getMode() == PM_SDCARD) {
          requestOnChange(SDPOS, clientId);
          requestOnChange(SDLEN, clientId);
          requestOnChange(SDSNUFFLE, clientId);
        }
        return;
        break;
      }

      case GETSYSTEM:
      {
        uint8_t quietFromH = static_cast<uint8_t>((config.store.clockTtsQuietFromMinutes / 60) % 24);
        uint8_t quietFromM = static_cast<uint8_t>(config.store.clockTtsQuietFromMinutes % 60);
        uint8_t quietToH = static_cast<uint8_t>((config.store.clockTtsQuietToMinutes / 60) % 24);
        uint8_t quietToM = static_cast<uint8_t>(config.store.clockTtsQuietToMinutes % 60);
        char quietFrom[6];
        char quietTo[6];
        quietFrom[0] = static_cast<char>('0' + (quietFromH / 10));
        quietFrom[1] = static_cast<char>('0' + (quietFromH % 10));
        quietFrom[2] = ':';
        quietFrom[3] = static_cast<char>('0' + (quietFromM / 10));
        quietFrom[4] = static_cast<char>('0' + (quietFromM % 10));
        quietFrom[5] = '\0';
        quietTo[0] = static_cast<char>('0' + (quietToH / 10));
        quietTo[1] = static_cast<char>('0' + (quietToH % 10));
        quietTo[2] = ':';
        quietTo[3] = static_cast<char>('0' + (quietToM / 10));
        quietTo[4] = static_cast<char>('0' + (quietToM % 10));
        quietTo[5] = '\0';
        int wsPos = snprintf(
          wsBuf,
          sizeof(wsBuf),
          "{\"sst\":%d,\"aif\":%d,\"rssiastext\":%d,\"vu\":%d,\"vupeak\":%d,\"vubox\":%d,\"softr\":%d,\"vut\":%d,\"mdns\":\"%s\",\"ipaddr\":\"%s\", \"watchdog\": %d, \"stallwatchdog\": %d, "
          "\"nameday\": %d, \"clocktts\": %d, \"clockttslang\": \"%.2s\", \"clockttsinterval\": %u, ",
          config.store.smartstart != 2, config.store.audioinfo, config.store.rssiAsText, config.store.vumeter, config.store.vuPeak, config.store.vuBidirectional, config.store.softapdelay, config.vuRefLevel, config.store.mdnsname,
          config.ipToStr(WiFi.localIP()), config.store.watchdog, config.store.stallWatchdog, config.store.nameday,
          config.store.clockTtsEnabled, config.store.clockTtsLanguage, static_cast<unsigned int>(config.store.clockTtsIntervalMinutes)
        );
        if (wsPos > 0 && static_cast<size_t>(wsPos) < sizeof(wsBuf)) {
          snprintf(
            wsBuf + wsPos,
            sizeof(wsBuf) - static_cast<size_t>(wsPos),
            "\"clockttsnostream\": %d, \"clockttsquietenabled\": %d, \"clockttsquietfrom\": \"%s\", \"clockttsquietto\": \"%s\" }",
            config.store.clockTtsOnlyWhenNoStream,
            config.store.clockTtsQuietHoursEnabled,
            quietFrom,
            quietTo
          );
        }
        break;
      }
      case GETSCREEN:
  sprintf(
    wsBuf,
    "{\"flip\":%d,\"inv\":%d,\"nump\":%d,\"plmc\":%d,\"dcc\":%d,\"slrt\":%d,\"tsf\":%d,\"tsd\":%d,\"dspon\":%d,"
    "\"tsmx\":%d,\"tsmy\":%d,"
    "\"br\":%d,\"con\":%d,"
    "\"scre\":%d,\"scrt\":%d,\"scrb\":%d,"
    "\"scrpe\":%d,\"scrpt\":%d,\"scrpb\":%d,"
    "\"fadeenabled\":%d,\"fadestartdelay\":%d,\"fadetarget\":%d,\"fadestep\":%d"
    "}",
    config.store.flipscreen,
    config.store.invertdisplay,
    config.store.numplaylist,
    config.store.playlistMovingCursor,
    config.store.directChannelChange,
    config.store.stationsListReturnTime,
    config.store.fliptouch,
    config.store.dbgtouch,
    config.store.dspon,
    config.store.xTouchMirroring,
    config.store.yTouchMirroring,
    config.store.brightness,
    config.store.contrast,
    config.store.screensaverEnabled,
    config.store.screensaverTimeout,
    config.store.screensaverBlank,
    config.store.screensaverPlayingEnabled,
    config.store.screensaverPlayingTimeout,
    config.store.screensaverPlayingBlank,
    config.store.fadeEnabled,
    config.store.fadeStartDelay,
    config.store.fadeTarget,
    config.store.fadeStep
  );
  break;
      case GETTIMEZONE:
      {
        const bool monoEnabled = (config.store.clockFontStyle == CLOCKFONT_STYLE_DIGI7) && config.store.clockFontMono;
        sprintf(
          wsBuf, "{\"tzh\":%d,\"tzm\":%d,\"sntp1\":\"%s\",\"sntp2\":\"%s\",\"timeint\":%d,\"timeintrtc\":%d,\"dateformat\":%d,\"clockfont\":%u,\"clockfontmono\":%d,\"clockampm\":%d}",
          config.store.tzHour, config.store.tzMin, config.store.sntp1, config.store.sntp2,
          config.store.timeSyncInterval, config.store.timeSyncIntervalRTC, config.store.dateFormat,
          static_cast<unsigned int>(config.store.clockFontStyle), monoEnabled, config.store.clockAmPmStyle
        );
        break;
      }
      case GETWEATHER:
        sprintf(
          wsBuf, "{\"wen\":%d,\"wlat\":\"%s\",\"wlon\":\"%s\",\"wkey\":\"%s\",\"wint\":%d}", config.store.showweather, config.store.weatherlat,
          config.store.weatherlon, config.store.weatherkey, config.store.weatherSyncInterval
        );
        break;
      case GETCONTROLS:
      {
        const bool dualEncodersConfigured = (ENC_BTNL != 255 && ENC_BTNR != 255) && (ENC2_BTNL != 255 && ENC2_BTNR != 255);
        sprintf(
          wsBuf, "{\"irtl\":%d,\"encind\":%d,\"encindavail\":%d,\"skipup\":%d}", config.store.irtlp,
          dualEncodersConfigured ? config.store.encodersIndependent : 0, dualEncodersConfigured, config.store.skipPlaylistUpDown
        );
        break;
      }
      case DSPON: sprintf(wsBuf, "{\"dspontrue\":%d}", 1); break;
      case STATION:
        requestOnChange(STATIONNAME, clientId);
        requestOnChange(ITEM, clientId);
        break;
      case STATIONNAME: sprintf(wsBuf, "{\"payload\":[{\"id\":\"nameset\", \"value\": \"%s\"}]}", config.station.name); break;
      case ITEM:        sprintf(wsBuf, "{\"current\": %d}", config.lastStation()); break;
      case TITLE:
        sprintf(wsBuf, "{\"payload\":[{\"id\":\"meta\", \"value\": \"%s\"}]}", config.station.title);
        Serial.printf("##CLI.META#: %s\r\n> ", config.station.title);
        break;
      case VOLUME:
        sprintf(wsBuf, "{\"payload\":[{\"id\":\"volume\", \"value\": %d}]}", config.store.volume);
        Serial.printf("##CLI.VOL#: %d\r\n", config.store.volume);
        break;
      case NRSSI:
        sprintf(
          wsBuf, "{\"payload\":[{\"id\":\"rssi\", \"value\": %d}, {\"id\":\"heap\", \"value\": %d}]}", rssi,
          (player.isRunning() && config.store.audioinfo) ? (int)(100 * player.inBufferFilled() / playerBufMax) : 0
        ); /*rssi = 255;*/
        break;
      case SDPOS:
        //"módosítás" Itt adja át az SD kártya pozícióját a csúszkához és a számlálóhoz.
        sprintf(
          wsBuf, "{\"sdpos\": %lu,\"sdtpos\": %lu,\"sdtend\": %lu}", player.getAudioFilePosition(), player.getAudioCurrentTime(), player.getAudioFileDuration()
        );
        //Serial.printf("netserver.cpp-->wsBuf: %s \n", wsBuf);
        break;
      // Az mp3 fájlon belül a zenekezdeti byte és utolsó byte pozíciója.
      case SDLEN:
        sprintf(wsBuf, "{\"sdmin\": %lu,\"sdmax\": %lu}", player.sd_min, player.sd_max);  // Az audionanlersben kap értéket.
        //Serial.printf("netserver.cpp-->wsBuf: %s \n", wsBuf);
        break;
      case SDSNUFFLE: sprintf(wsBuf, "{\"snuffle\": %d}", config.store.sdsnuffle); break;
      case BITRATE:
        sprintf(
          wsBuf, "{\"payload\":[{\"id\":\"bitrate\", \"value\": %d}, {\"id\":\"fmt\", \"value\": \"%s\"}]}", config.station.bitrate, getFormat(config.configFmt)
        );
        break;
      case MODE:
        sprintf(wsBuf, "{\"payload\":[{\"id\":\"playerwrap\", \"value\": \"%s\"}]}", player.status() == PLAYING ? "playing" : "stopped");
        break;
      case EQUALIZER:
        sprintf(
          wsBuf, "{\"payload\":[{\"id\":\"bass\", \"value\": %d}, {\"id\": \"middle\", \"value\": %d}, {\"id\": \"trebble\", \"value\": %d}]}",
          config.store.bass, config.store.middle, config.store.trebble
        );
        break;
      case BALANCE: sprintf(wsBuf, "{\"payload\":[{\"id\": \"balance\", \"value\": %d}]}", config.store.balance); break;
      case SDINIT:  sprintf(wsBuf, "{\"sdinit\": %d}", SDC_CS != 255); break;
      case GETPLAYERMODE:
      {  //DLNA mod
#ifdef USE_DLNA
        if (config.getMode() == PM_WEB && config.store.playlistSource == PL_SRC_DLNA) {
          sprintf(wsBuf, "{\"playermode\": \"modedlna\"}");
        } else
#endif
          if (config.getMode() == PM_SDCARD) {
          sprintf(wsBuf, "{\"playermode\": \"modesd\"}");
        } else {
          sprintf(wsBuf, "{\"playermode\": \"modeweb\"}");
        }
        break;
      }
#ifdef USE_SD
      case CHANGEMODE: config.changeMode(config.newConfigMode);

  #ifdef USE_DLNA  //DLNA modplus
        if (config.resumeAfterModeChange) {
          uint16_t st = (config.getMode() == PM_SDCARD)
                          ? config.store.lastSdStation
                          : (config.store.playlistSource == PL_SRC_DLNA ? config.store.lastDlnaStation : config.store.lastStation);

          Serial.printf("[MODE] Resume playback → station %u\n", st);
          player.sendCommand({PR_PLAY, st});
          config.resumeAfterModeChange = false;
        }
  #endif  //DLNA modplus
        return;
        break;
#endif
      default: break;
    }
    if (strlen(wsBuf) > 0) {
      if (clientId == 0) {
        websocket.textAll(wsBuf);
      } else {
        websocket.text(clientId, wsBuf);
      }
#ifdef MQTT_ROOT_TOPIC
      if (clientId == 0 && (request.type == STATION || request.type == ITEM || request.type == TITLE || request.type == MODE)) {
        mqttPublishStatus();
      }
      if (clientId == 0 && request.type == VOLUME) {
        mqttPublishVolume();
      }
#endif
    }
  }
}

void NetServer::loop() {
  if (network.status == SDREADY) {
    return;
  }
  if (shouldReboot) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  processQueue();
  updateWifiScanCache();
  websocket.cleanupClients();
  switch (importRequest) {
    case IMPL:
      importPlaylist();
      importRequest = IMDONE;
      break;
    case IMWIFI:
      config.saveWifi();
      importRequest = IMDONE;
      break;
    default: break;
  }
  static uint32_t lastPing = 0;

if (millis() - lastPing > 5000) {  // 5 mp
    lastPing = millis();

    if (websocket.count() > 0) {
        websocket.textAll("{\"ping\":1}");
    }
}
static uint32_t lastWsActivity = 0;

// ha van kliens, frissítjük az aktivitást
if (websocket.count() > 0) {
    lastWsActivity = millis();
}

// ha 15 mp óta nincs aktivitás → zárjuk
if (millis() - lastWsActivity > 15000 && websocket.count() > 0) {
    Serial.println("[WS] Force reconnect");
    websocket.closeAll();
}

}

#if IR_PIN != 255
void NetServer::irToWs(const char *protocol, uint64_t irvalue) {
  wsBuf[0] = '\0';
  sprintf(wsBuf, "{\"ircode\": %llu, \"protocol\": \"%s\"}", irvalue, protocol);
  websocket.textAll(wsBuf);
}

void NetServer::irValsToWs() {
  if (!irRecordEnable) {
    return;
  }
  wsBuf[0] = '\0';
  sprintf(
    wsBuf, "{\"irvals\": [%llu, %llu, %llu]}", config.ircodes.irVals[config.irBtnId][0], config.ircodes.irVals[config.irBtnId][1],
    config.ircodes.irVals[config.irBtnId][2]
  );
  websocket.textAll(wsBuf);
}
#endif

void NetServer::onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (config.parseWsCommand((const char *)data, _wscmd, _wsval, 65)) {
      if (strcmp(_wscmd, "ping") == 0) {
        websocket.text(clientId, "{\"pong\": 1}");
        return;
      }
      // Tone settings (trebble/middle/bass)
      if (strcmp(_wscmd, "trebble") == 0) {
        int8_t valb = atoi(_wsval);
        config.setTone(config.store.bass, config.store.middle, valb);
        return;
      }
      if (strcmp(_wscmd, "middle") == 0) {
        int8_t valb = atoi(_wsval);
        config.setTone(config.store.bass, valb, config.store.trebble);
        return;
      }
      if (strcmp(_wscmd, "bass") == 0) {
        int8_t valb = atoi(_wsval);
        config.setTone(valb, config.store.middle, config.store.trebble);
        return;
      }
      if (strcmp(_wscmd, "submitplaylistdone") == 0) {
#ifdef MQTT_ROOT_TOPIC
        //mqttplaylistticker.attach(5, mqttplaylistSend);
        timekeeper.waitAndDo(5, mqttplaylistSend);
#endif
        if (player.isRunning()) {
          player.sendCommand({PR_PLAY, -config.lastStation()});
        }
        return;
      }

#ifdef USE_DLNA  //DLNA mod
      // ===== WEB playlist aktiválás =====
      if (strcmp(_wscmd, "playlist") == 0 && strcmp(_wsval, "web") == 0) {

        Serial.println("[WEB] Switch to WEB playlist");

        config.store.playlistSource = PL_SRC_WEB;
        config.saveValue(&config.store.playlistSource, (uint8_t)PL_SRC_WEB);

        config.indexPlaylist();
        config.initPlaylist();

        netserver.requestOnChange(GETINDEX, 0);
        netserver.requestOnChange(GETPLAYERMODE, 0);

        return;
      }
      // ===== DLNA playlist aktiválás =====
      if (strcmp(_wscmd, "playlist") == 0 && strcmp(_wsval, "dlna") == 0) {

        Serial.println("[WEB] Switch to DLNA playlist");

        config.store.playlistSource = PL_SRC_DLNA;
        config.saveValue(&config.store.playlistSource, (uint8_t)PL_SRC_DLNA);

        config.indexDLNAPlaylist();
        config.initDLNAPlaylist();

        netserver.requestOnChange(GETINDEX, 0);
        netserver.requestOnChange(GETPLAYERMODE, 0);

        return;
      }
#endif

      if (cmd.exec(_wscmd, _wsval, clientId)) {
        return;
      }
    }
  }
}

void NetServer::getPlaylist(uint8_t clientId) {
  //sprintf(nsBuf, "{\"file\": \"http://%s%s\"}", config.ipToStr(WiFi.localIP()), PLAYLIST_PATH);
  sprintf(nsBuf, "{\"file\": \"http://%s%s\"}", config.ipToStr(WiFi.localIP()), REAL_PLAYL);  //DLNA mod
  if (clientId == 0) {
    websocket.textAll(nsBuf);
  } else {
    websocket.text(clientId, nsBuf);
  }
}

int NetServer::_readPlaylistLine(File &file, char *line, size_t size) {
  int bytesRead = file.readBytesUntil('\n', line, size);
  if (bytesRead > 0) {
    line[bytesRead] = 0;
    if (line[bytesRead - 1] == '\r') {
      line[bytesRead - 1] = 0;
    }
  }
  return bytesRead;
}

bool NetServer::importPlaylist() {
  if (config.getMode() == PM_SDCARD) {
    return false;
  }
  //player.sendCommand({PR_STOP, 0});
  File tempfile = LittleFS.open(TMP_PATH, "r");
  if (!tempfile) {
    return false;
  }
  char linePl[BUFLEN * 3];
  int sOvol;
  _readPlaylistLine(tempfile, linePl, sizeof(linePl) - 1);
  if (config.parseCSV(linePl, nsBuf, nsBuf2, sOvol)) {
    tempfile.close();
    LittleFS.rename(TMP_PATH, PLAYLIST_PATH);
    requestOnChange(PLAYLISTSAVED, 0);
    return true;
  }
  if (config.parseJSON(linePl, nsBuf, nsBuf2, sOvol)) {
    File playlistfile = LittleFS.open(PLAYLIST_PATH, "w");
    snprintf(linePl, sizeof(linePl) - 1, "%s\t%s\t%d", nsBuf, nsBuf2, 0);
    playlistfile.println(linePl);
    while (tempfile.available()) {
      _readPlaylistLine(tempfile, linePl, sizeof(linePl) - 1);
      if (config.parseJSON(linePl, nsBuf, nsBuf2, sOvol)) {
        snprintf(linePl, sizeof(linePl) - 1, "%s\t%s\t%d", nsBuf, nsBuf2, 0);
        playlistfile.println(linePl);
      }
    }
    playlistfile.flush();
    playlistfile.close();
    tempfile.close();
    LittleFS.remove(TMP_PATH);
    requestOnChange(PLAYLISTSAVED, 0);
    return true;
  }
  tempfile.close();
  LittleFS.remove(TMP_PATH);
  return false;
}

void NetServer::requestOnChange(requestType_e request, uint8_t clientId) {
  if (nsQueue == NULL) {
    return;
  }
  nsRequestParams_t nsrequest;
  nsrequest.type = request;
  nsrequest.clientId = clientId;
  xQueueSend(nsQueue, &nsrequest, NSQ_SEND_DELAY);
}

void NetServer::resetQueue() {
  if (nsQueue != NULL) {
    xQueueReset(nsQueue);
  }
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  static int freeSpace = 0;
  if (request->url() == "/upload") {
    if (!index) {
      if (filename != "tempwifi.csv") {
        //player.sendCommand({PR_STOP, 0});
        if (LittleFS.exists(PLAYLIST_PATH)) {
          LittleFS.remove(PLAYLIST_PATH);
        }
        if (LittleFS.exists(INDEX_PATH)) {
          LittleFS.remove(INDEX_PATH);
        }
        if (LittleFS.exists(PLAYLIST_SD_PATH)) {
          LittleFS.remove(PLAYLIST_SD_PATH);
        }
        if (LittleFS.exists(INDEX_SD_PATH)) {
          LittleFS.remove(INDEX_SD_PATH);
        }
      }
      freeSpace = (float)LittleFS.totalBytes() / 100 * 68 - LittleFS.usedBytes();
      request->_tempFile = LittleFS.open(TMP_PATH, "w");
    } else {
    }
    if (len) {
      if (freeSpace > index + len) {
        request->_tempFile.write(data, len);
      }
    }
    if (final) {
      request->_tempFile.close();
      freeSpace = 0;
    }
  } else if (request->url() == "/update") {
    if (!index) {
      int target = (request->getParam("updatetarget", true)->value() == "littlefs") ? U_LITTLEFS : U_FLASH;
      Serial.printf("Update Start: %s\n", filename.c_str());
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, target)) {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
  } else {  // "/webboard"
    DBGVB("File: %s, size:%u bytes, index: %u, final: %s\n", filename.c_str(), len, index, final ? "true" : "false");
    String safeFilename = filename;
    safeFilename.replace("\\", "/");
    int slashPos = safeFilename.lastIndexOf('/');
    if (slashPos >= 0) {
      safeFilename = safeFilename.substring(slashPos + 1);
    }
    String lowerFilename = safeFilename;
    lowerFilename.toLowerCase();
    if (safeFilename.length() == 0) {
      g_webboardUploadHadError = true;
      return;
    }
    if (!index) {
      player.sendCommand({PR_STOP, 0});
      String spath = "/www/";
      if (!LittleFS.exists("/www")) {
        LittleFS.mkdir("/www");
      }
      if (lowerFilename == "playlist.csv" || lowerFilename == "wifi.csv") {
        spath = "/data/";
        if (!LittleFS.exists("/data")) {
          LittleFS.mkdir("/data");
        }
      } else if (lowerFilename.endsWith(".vlw")) {
        spath = "/fonts/";
        if (!LittleFS.exists("/fonts")) {
          LittleFS.mkdir("/fonts");
        }
      } else if (lowerFilename.endsWith(".png") || lowerFilename.endsWith(".jpg") || lowerFilename.endsWith(".jpeg")
                 || lowerFilename.endsWith(".gif") || lowerFilename.endsWith(".webp") || lowerFilename.endsWith(".bmp")
                 || lowerFilename.endsWith(".svg") || lowerFilename.endsWith(".ico")) {
        spath = "/images/";
        if (!LittleFS.exists("/images")) {
          LittleFS.mkdir("/images");
        }
      }
      request->_tempFile = LittleFS.open(spath + safeFilename, "w");
      if (!request->_tempFile) {
        g_webboardUploadHadError = true;
        Serial.printf("[WEBBOARD] open failed: %s\n", (spath + safeFilename).c_str());
      }
    }
    if (len && request->_tempFile) {
      if (request->_tempFile.write(data, len) != len) {
        g_webboardUploadHadError = true;
      }
    }
    if (final && request->_tempFile) {
      request->_tempFile.close();
      if (lowerFilename == "playlist.csv") {
        config.indexPlaylist();
      }
    }
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: /*netserver.requestOnChange(STARTUP, client->id()); */
      if (config.store.audioinfo) {
        Serial.printf("[WEBSOCKET] client #%lu connected from %s\n", client->id(), config.ipToStr(client->remoteIP()));
      }
      break;
    case WS_EVT_DISCONNECT:
      if (config.store.audioinfo) {
        Serial.printf("[WEBSOCKET] client #%lu disconnected\n", client->id());
      }
      break;
    case WS_EVT_DATA:  netserver.onWsMessage(arg, data, len, client->id()); break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR: break;
  }
}
void handleNotFound(AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
  if (network.status == CONNECTED) {
    if (request->url() == "/logout") {
      request->send(401);
      return;
    }
  }
  if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
    return request->requestAuthentication();
  }
#endif
  if (shouldRedirectEmptyFsRequest(request)) {
    request->redirect("/");
    return;
  }
  if (request->url() == "/emergency") {
    request->send_P(200, "text/html", emergency_form);
    return;
  }
  if (request->method() == HTTP_POST && request->url() == "/webboard" && config.emptyFS) {
    if (g_webboardUploadHadError) {
      Serial.println("[WEBBOARD] Upload had file write/open error, staying on webboard page");
      g_webboardUploadHadError = false;
      request->redirect("/webboard");
      return;
    }

    if (!hasRequiredWebboardFiles()) {
      Serial.println("[WEBBOARD] Upload incomplete, required /www and /fonts files are missing");
      request->redirect("/webboard");
      return;
    }

    request->redirect("/");
    ESP.restart();
    return;
  }
  if (request->method() == HTTP_GET) {
    //DLNA mod
#ifdef USE_DLNA
    if (request->method() == HTTP_GET && request->url().startsWith("/data/dlna_")) {

      String path = request->url();
      Serial.printf("[DLNA][HTTP] GET %s\n", path.c_str());

      if (!LittleFS.exists(path)) {
        request->send(404, "text/plain", "DLNA file not found");
        return;
      }

      String type = "text/plain";
      if (path.endsWith(".json")) {
        type = "application/json";
      } else if (path.endsWith(".csv")) {
        type = "text/plain";
      }

      request->send(LittleFS, path, type);
      return;
    }
#endif
    DBGVB("[%s] client ip=%s request of %s", __func__, config.ipToStr(request->client()->remoteIP()), request->url().c_str());
    if (
      strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 || strcmp(request->url().c_str(), SSIDS_PATH) == 0 || strcmp(request->url().c_str(), INDEX_PATH) == 0
      || strcmp(request->url().c_str(), TMP_PATH) == 0 || strcmp(request->url().c_str(), PLAYLIST_SD_PATH) == 0
      || strcmp(request->url().c_str(), INDEX_SD_PATH) == 0
#ifdef USE_DLNA  //DLNA mod
      || strcmp(request->url().c_str(), PLAYLIST_DLNA_PATH) == 0 || strcmp(request->url().c_str(), INDEX_DLNA_PATH) == 0
#endif
    ) {
#ifdef MQTT_ROOT_TOPIC
      if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0) {
        while (mqttplaylistblock) {
          vTaskDelay(5);
        }
      }
#endif
      /* if(strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 && config.getMode()==PM_SDCARD){
        netserver.chunkedHtmlPage("application/octet-stream", request, PLAYLIST_SD_PATH);
      }else{
        netserver.chunkedHtmlPage("application/octet-stream", request, request->url().c_str());
      }*/
      if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0) {
        netserver.chunkedHtmlPage("application/octet-stream", request, REAL_PLAYL);  //DLNA mod
        return;
      }

      if (strcmp(request->url().c_str(), INDEX_PATH) == 0) {
        netserver.chunkedHtmlPage("application/octet-stream", request, REAL_INDEX);  //DLNA mod
        return;
      }
      netserver.chunkedHtmlPage("application/octet-stream", request, request->url().c_str());
      return;
    }  // if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 ||
  }  // if (request->method() == HTTP_GET)

  if (request->method() == HTTP_POST) {
    if (request->url() == "/webboard") {
      request->redirect("/");
      return;
    }  // <--post files from /data/www
    if (request->url() == "/upload") {  // <--upload playlist.csv or wifi.csv
      if (request->hasParam("plfile", true, true)) {
        netserver.importRequest = IMPL;
        request->send(200);
      } else if (request->hasParam("wifile", true, true)) {
        netserver.importRequest = IMWIFI;
        request->send(200);
      } else {
        request->send(404);
      }
      return;
    }
    if (request->url() == "/update") {  // <--upload firmware
      shouldReboot = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : updateError());
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
  }  // if (request->method() == HTTP_POST)

  if (request->url() == "/favicon.ico") {
    request->send(200, "image/x-icon", "data:,");
    return;
  }
  if (request->url() == "/wifiscan") {
    startWifiScanIfNeeded();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", g_wifiScanCache);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("X-WiFi-Scan-Running", g_wifiScanRunning ? "1" : "0");
    request->send(response);
    return;
  }
  if (request->url() == "/variables.js") {  //DLNA mod
    snprintf(
      netserver.nsBuf, sizeof(netserver.nsBuf),
      "var fwVersion='%s';\n"
      "var formAction='%s';\n"
      "var playMode='%s';\n"
    "var isStaConnected=%d;\n"
      "var dlnaSupported=%d;\n",
      FW_VERSION, (network.status == CONNECTED && !config.emptyFS) ? "webboard" : "", (network.status == CONNECTED) ? "player" : "ap",
    (network.status == CONNECTED) ? 1 : 0,
#ifdef USE_DLNA
      1
#else
      0
#endif
    );
      AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", netserver.nsBuf);
      response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
      response->addHeader("Pragma", "no-cache");
      response->addHeader("Expires", "0");
      request->send(response);
    return;
  }
  if (strcmp(request->url().c_str(), "/settings.html") == 0 || strcmp(request->url().c_str(), "/update.html") == 0
      || strcmp(request->url().c_str(), "/ir.html") == 0) {
    //request->send_P(200, "text/html", index_html);
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
    response->addHeader("Cache-Control", "max-age=31536000");
    request->send(response);
    return;
  }
  if (request->method() == HTTP_GET && request->url() == "/webboard") {
    request->send_P(200, "text/html", emptyfs_html);
    return;
  }
  Serial.print("Not Found: ");
  Serial.println(request->url());
  request->send(404, "text/plain", "Not found");
}

void handleIndex(AsyncWebServerRequest *request) {
  if (config.emptyFS) {
    if (request->url() == "/" && request->method() == HTTP_GET) {
      request->send_P(200, "text/html", emptyfs_html);
      return;
    }
    if (request->url() == "/" && request->method() == HTTP_POST) {
      String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : request->arg("ssid");
      String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : request->arg("pass");
      if (ssid != "" && pass != "") {
        netserver.nsBuf[0] = '\0';
        snprintf(netserver.nsBuf, sizeof(netserver.nsBuf), "%s\t%s", ssid.c_str(), pass.c_str());
        request->redirect("/");
        config.saveWifiFromNextion(netserver.nsBuf);
        return;
      }
      request->redirect("/");
      ESP.restart();
      return;
    }
    Serial.print("Not Found: ");
    Serial.println(request->url());
    request->send(404, "text/plain", "Not found");
    return;
  }  // end if(config.emptyFS)
#if defined(HTTP_USER) && defined(HTTP_PASS)
  if (network.status == CONNECTED) {
    if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
      return request->requestAuthentication();
    }
  }
#endif
  if (strcmp(request->url().c_str(), "/") == 0 && request->params() == 0) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
    response->addHeader("Cache-Control", "max-age=31536000");
    request->send(response);
    return;
  }
  if (network.status == CONNECTED) {
    int paramsNr = request->params();
    if (paramsNr == 1) {
      AsyncWebParameter *p = request->getParam(0);
      if (cmd.exec(p->name().c_str(), p->value().c_str())) {
        if (p->name() == "reset" || p->name() == "clearlittlefs") {
          request->redirect("/");
        }
        if (p->name() == "clearlittlefs") {
          delay(100);
          ESP.restart();
        }
        request->send(200, "text/plain", "");
        return;
      }
    }
    if (request->hasArg("trebble") && request->hasArg("middle") && request->hasArg("bass")) {
      config.setTone(request->getParam("bass")->value().toInt(), request->getParam("middle")->value().toInt(), request->getParam("trebble")->value().toInt());
      request->send(200, "text/plain", "");
      return;
    }
    if (request->hasArg("sleep")) {
      int sford = request->getParam("sleep")->value().toInt();
      int safterd = request->hasArg("after") ? request->getParam("after")->value().toInt() : 0;
      if (sford > 0 && safterd >= 0) {
        request->send(200, "text/plain", "");
        config.sleepForAfter(sford, safterd);
        return;
      }
    }
    request->send(404, "text/plain", "Not found");

  } else {
    request->send(404, "text/plain", "Not found");
  }
}
