#include "../core/options.h"
#ifdef USE_DLNA

#include "../core/config.h"
#include "dlna_worker.h"
#include "dlna_service.h"
#include <LittleFS.h>
#include <WiFi.h>
#include "esp_task_wdt.h"
#include "dlna_index.h"
#include "dlna_http_guard.h" 

QueueHandle_t g_dlnaQueue = nullptr;
SemaphoreHandle_t g_littlefsMux = nullptr;
DlnaStatus g_dlnaStatus = {false,false,0,0,0,{0},0,0};

static TaskHandle_t s_workerTask = nullptr;
static uint32_t s_playlistVer = 1;

uint32_t dlna_playlist_version() { return s_playlistVer; }

static void bumpPlaylistVer() {
  s_playlistVer++;
  if (s_playlistVer == 0) s_playlistVer = 1;
  g_dlnaStatus.playlistVer = s_playlistVer;
}

void dlna_status_setBusy(const DlnaJob& j, const char* msg) {
  g_dlnaStatus.busy = true;
  g_dlnaStatus.ok = false;
  g_dlnaStatus.err = 0;
  g_dlnaStatus.reqId = j.reqId;
  g_dlnaStatus.scanNext = 0;
  g_dlnaStatus.scanTotal = 0;
  strncpy(g_dlnaStatus.msg, msg ? msg : "busy", sizeof(g_dlnaStatus.msg)-1);
  g_dlnaStatus.msg[sizeof(g_dlnaStatus.msg)-1] = 0;
}

void dlna_status_setDone(const DlnaJob& j, bool ok, int err, const char* msg) {
  g_dlnaStatus.busy = false;
  g_dlnaStatus.ok = ok;
  g_dlnaStatus.err = err;
  g_dlnaStatus.reqId = j.reqId;
  strncpy(g_dlnaStatus.msg, msg ? msg : (ok ? "ok" : "fail"), sizeof(g_dlnaStatus.msg)-1);
  g_dlnaStatus.msg[sizeof(g_dlnaStatus.msg)-1] = 0;
}

void dlna_status_progress(const char* msg) {
  if (!g_dlnaStatus.busy) return;
  strncpy(g_dlnaStatus.msg, msg ? msg : "", sizeof(g_dlnaStatus.msg)-1);
  g_dlnaStatus.msg[sizeof(g_dlnaStatus.msg)-1] = 0;
}

// true: van közvetlen track; false: a job státusza már lezárva (422 = nincs, 423 = szünet, a UI dönthet a folytatásról)
static bool scan_for_tracks(const DlnaJob& j, DlnaIndex& idx, const String& ctlUrl, bool& hasItems) {
  bool hasContainers = j.start > 0;
  uint32_t next = 0, total = 0;
  hasItems = false;

  if (!idx.browseAndDecide(ctlUrl, j.objectId, hasItems, hasContainers, j.start, &next, &total)) {
    dlna_status_setDone(j, false, 500, "browse failed");
    return false;
  }
  if (hasItems) return true;

  if (next > 0) {
    char buf[96];
    snprintf(buf, sizeof(buf), "No tracks in the first %u of %u entries yet", (unsigned)next, (unsigned)total);
    g_dlnaStatus.scanNext = next;
    g_dlnaStatus.scanTotal = total;
    dlna_status_setDone(j, false, 423, buf);
    return false;
  }

  dlna_status_setDone(j, false, 422, hasContainers ? "No tracks in this container (only subfolders)" : "Empty container");
  return false;
}

/*static bool write_playlist_atomic(const char* tmpPath, const char* finalPath, const String& content) {
  // mutex: egy időben ne olvassa/írja más
  xSemaphoreTake(g_littlefsMux, portMAX_DELAY);

  File f = LittleFS.open(tmpPath, FILE_WRITE);
  if (!f) { xSemaphoreGive(g_littlefsMux); return false; }
  size_t n = f.print(content);
  f.close();
  if (n != content.length()) { LittleFS.remove(tmpPath); xSemaphoreGive(g_littlefsMux); return false; }

  LittleFS.remove(finalPath);
  bool ok = LittleFS.rename(tmpPath, finalPath);

  xSemaphoreGive(g_littlefsMux);
  return ok;
}

static bool append_playlist_atomic(const char* tmpPath, const char* finalPath, const String& addLines) {
  xSemaphoreTake(g_littlefsMux, portMAX_DELAY);

  // 1) read old
  String old;
  {
    File f = LittleFS.open(finalPath, FILE_READ);
    if (f) { old = f.readString(); f.close(); }
  }

  // 2) write merged to tmp
  File t = LittleFS.open(tmpPath, FILE_WRITE);
  if (!t) { xSemaphoreGive(g_littlefsMux); return false; }
  t.print(old);
  if (old.length() && old[old.length()-1] != '\n') t.print("\n");
  t.print(addLines);
  t.close();

  // 3) swap
  LittleFS.remove(finalPath);
  bool ok = LittleFS.rename(tmpPath, finalPath);

  xSemaphoreGive(g_littlefsMux);
  return ok;
}

*/
// WDT/yield helper hosszú ciklusokba
static inline void worker_yield() {
  //esp_task_wdt_reset();
  vTaskDelay(1); // 1 tick yield
}

// A TMP_PATH fájl tartalmával lecseréli a DLNA playlistet
static void commit_tmp_as_build(const DlnaJob& j) {
  xSemaphoreTake(g_littlefsMux, portMAX_DELAY);
  LittleFS.remove(PLAYLIST_DLNA_PATH);
  bool ok = LittleFS.rename(TMP_PATH, PLAYLIST_DLNA_PATH);
  xSemaphoreGive(g_littlefsMux);

  if (!ok) {
    dlna_status_setDone(j, false, 550, "rename failed");
    return;
  }

  config.sdResumePos = 0;
  config.resumeAfterModeChange = false;

  // 🔑 DLNA build -> reset DLNA index ONLY
  config.store.lastDlnaStation = 1;
  config.saveValue(&config.store.lastDlnaStation, (uint16_t)1);

  config.indexDLNAPlaylist();   // az index nélkül a régi (rövid) index maradna érvényben
  bumpPlaylistVer();
  dlna_status_setDone(j, true, 0, "build ok");
}

// A TMP_PATH fájl tartalmát hozzáfűzi a DLNA playlisthez
static void commit_tmp_as_append(const DlnaJob& j) {
  xSemaphoreTake(g_littlefsMux, portMAX_DELAY);

  bool needNl = false;
  {
    File chk = LittleFS.open(PLAYLIST_DLNA_PATH, FILE_READ);
    if (chk && chk.size() > 0) {
      chk.seek(chk.size() - 1);
      needNl = (chk.read() != '\n');
    }
    if (chk) chk.close();
  }

  File out = LittleFS.open(PLAYLIST_DLNA_PATH, FILE_APPEND);
  File in  = LittleFS.open(TMP_PATH, FILE_READ);

  bool ok = false;
  if (out && in) {
    if (needNl) out.print("\n");
    while (in.available()) out.write(in.read());
    ok = true;
  }

  if (out) out.close();
  if (in)  in.close();
  LittleFS.remove(TMP_PATH);

  xSemaphoreGive(g_littlefsMux);

  if (!ok) {
    dlna_status_setDone(j, false, 551, "append failed");
    return;
  }

  config.indexDLNAPlaylist();
  bumpPlaylistVer();
  dlna_status_setDone(j, true, 0, "append ok");
}

// ===== Kijelölt trackek (POST /dlna/sel) =====
static String s_selBody;
static bool s_selOverflow = false;

void dlna_sel_chunk(const uint8_t* data, size_t len, size_t index) {
  const size_t kMaxSel = 24 * 1024;
  if (index == 0) { s_selBody = String(); s_selOverflow = false; }
  if (s_selOverflow) return;
  if (s_selBody.length() + len > kMaxSel) { s_selOverflow = true; s_selBody = String(); return; }
  s_selBody.concat((const char*)data, len);
}

bool dlna_sel_overflow() { return s_selOverflow; }
bool dlna_sel_empty() { return s_selBody.length() == 0; }

// ===== Aszinkron list állapot =====
static SemaphoreHandle_t s_listMux = nullptr;
static String   s_listJson;
static uint32_t s_listReq = 0;
static int      s_listState = 0;   // 0 nincs, 1 folyamatban, 2 kész, 3 hiba
static uint32_t s_listSubmitMs = 0;

bool dlna_list_submit(const char* objectId, uint32_t start, uint32_t reqId) {
  if (!s_listMux || !g_dlnaQueue) return false;
  if (xSemaphoreTake(s_listMux, pdMS_TO_TICKS(100)) != pdTRUE) return false;

  // elakadt (15 s-nél régebbi) kérést nem tekintünk foglaltnak
  if (s_listState == 1 && millis() - s_listSubmitMs < 15000) {
    xSemaphoreGive(s_listMux);
    return false;
  }

  DlnaJob j{};
  j.type = DJ_LIST;
  strlcpy(j.objectId, objectId, sizeof(j.objectId));
  j.reqId = reqId;
  j.start = start;

  s_listJson = "";
  s_listReq = reqId;
  s_listState = 1;
  s_listSubmitMs = millis();

  bool ok = xQueueSend(g_dlnaQueue, &j, 0) == pdTRUE;
  if (!ok) s_listState = 0;
  xSemaphoreGive(s_listMux);
  return ok;
}

int dlna_list_result(uint32_t reqId, String& out) {
  if (!s_listMux) return -2;
  if (xSemaphoreTake(s_listMux, pdMS_TO_TICKS(100)) != pdTRUE) return 0;

  int rc;
  if (s_listReq != reqId || s_listState == 0) {
    rc = -2;
  } else if (s_listState == 1) {
    rc = (millis() - s_listSubmitMs > 15000) ? -1 : 0;
  } else if (s_listState == 2) {
    out = s_listJson;
    rc = 1;
  } else {
    rc = -1;
  }
  xSemaphoreGive(s_listMux);
  return rc;
}

static void list_finish(uint32_t reqId, bool ok, String& json) {
  if (xSemaphoreTake(s_listMux, portMAX_DELAY) != pdTRUE) return;
  if (s_listReq == reqId) {
    s_listJson = ok ? json : String();
    s_listState = ok ? 2 : 3;
  }
  xSemaphoreGive(s_listMux);
}

static void dlna_worker_task(void* ) {
  //esp_task_wdt_add(nullptr); // add current task to WDT (ha használod)
  DlnaJob j{};

  for (;;) {
    if (xQueueReceive(g_dlnaQueue, &j, portMAX_DELAY) != pdTRUE) continue;

    Serial.printf("[DLNA][WORK] job=%d objectId='%s' heap=%u maxblk=%u rssi=%d\n", (int)j.type, j.objectId, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (int)WiFi.RSSI());

    if (j.type == DJ_CANCEL) {
      dlna_status_setDone(j, true, 0, "cancelled");
      continue;
    }

    if (j.type == DJ_LIST) {
      String json;
      bool lok = false;
      const String ctlUrl = dlnaGetControlUrl();
      if (ctlUrl.length()) {
        DlnaIndex idx;
        lok = idx.listContainer(ctlUrl, String(j.objectId), json, j.start);
      }
      list_finish(j.reqId, lok, json);
      worker_yield();
      continue;
    }

    dlna_status_setBusy(j, "working");

    bool ok = false;
   // int err = 0;

    // !!! FONTOS: itt semmilyen AsyncWebServerRequest nincs, csak paraméterek
    if (j.type == DJ_BUILD) {
      dlna_status_setBusy(j, "build");

      const String ctlUrl = dlnaGetControlUrl();
      if (!ctlUrl.length()) {
        dlna_status_setDone(j, false, 503, "DLNA not initialized");
        continue;
      }

      DlnaIndex idx;
      bool hasItems = false;

      if (!scan_for_tracks(j, idx, ctlUrl, hasItems)) continue;
      ok = true;

      // Csak a megjelenített mappa tartalma kerül a listába, almappákba nem lépünk be.
      const uint8_t depth = 0;

      uint32_t limit = j.hardLimit ? j.hardLimit : 20000;

      ok = idx.autoBuildPlaylist(
             ctlUrl,
             j.objectId,
             depth,
             limit    // ha nincs: add meg defaultnak
           );

      if (!ok) {
        dlna_status_setDone(j, false, 500, "build failed");
        continue;
      }

      commit_tmp_as_build(j);
    }

    else if (j.type == DJ_APPEND) {
      dlna_status_setBusy(j, "append");

      const String ctlUrl = dlnaGetControlUrl();
      if (!ctlUrl.length()) {
        dlna_status_setDone(j, false, 503, "DLNA not initialized");
        continue;
      }

      DlnaIndex idx;
      bool hasItems = false;

      if (!scan_for_tracks(j, idx, ctlUrl, hasItems)) continue;
      ok = true;

      // Csak a megjelenített mappa tartalma kerül a listába, almappákba nem lépünk be.
      const uint8_t depth = 0;

      uint32_t limit = j.hardLimit ? j.hardLimit : 20000;

      ok = idx.autoBuildPlaylist(
             ctlUrl,
             j.objectId,
             depth,
             limit
           );

      if (!ok) {
        dlna_status_setDone(j, false, 500, "append build failed");
        continue;
      }

      commit_tmp_as_append(j);
    }

    else if (j.type == DJ_BUILD_SEL || j.type == DJ_APPEND_SEL) {
      const bool isAppend = (j.type == DJ_APPEND_SEL);
      dlna_status_setBusy(j, isAppend ? "append" : "build");

      String body = s_selBody;
      s_selBody = String();

      DlnaIndex idx;
      uint32_t written = 0;
      File f = LittleFS.open(TMP_PATH, "w");
      if (!f) {
        dlna_status_setDone(j, false, 500, "tmp open failed");
        continue;
      }

      int pos = 0;
      while (pos < (int)body.length()) {
        int eol = body.indexOf('\n', pos);
        if (eol < 0) eol = body.length();
        String line = body.substring(pos, eol);
        pos = eol + 1;

        int tab = line.indexOf('\t');
        if (tab <= 0) continue;
        String title = line.substring(0, tab);
        String url = line.substring(tab + 1);
        title.trim();
        url.trim();
        url.replace("\t", "");
        if (!title.length() || !url.length()) continue;

        String nice = idx.beautifyTitle(title);
        nice.replace("\t", " ");
        f.printf("%s\t%s\t0\n", nice.c_str(), url.c_str());
        written++;
      }
      f.close();

      if (written == 0) {
        LittleFS.remove(TMP_PATH);
        dlna_status_setDone(j, false, 422, "No tracks selected");
        continue;
      }

      if (isAppend) commit_tmp_as_append(j);
      else commit_tmp_as_build(j);
    }

else if (j.type == DJ_INIT) {
  dlna_status_setBusy(j, "init");

  String errStr;
  String rootId = String(dlnaIDX);

  vTaskDelay(1);

  bool okInit = dlnaInit(rootId, errStr);

  vTaskDelay(1);

  dlna_status_setDone(
    j,
    okInit,
    okInit ? 0 : 503,
    okInit ? "init ok" : errStr.c_str()
  );
}

    worker_yield();
  }
}

void dlna_worker_start() {
  if (s_workerTask) return;                 // már fut
  if (!g_dlnaHttpMux) {
	  g_dlnaHttpMux = xSemaphoreCreateMutex();
	  Serial.println("[DLNA] HTTP mutex created");}
  if (!g_littlefsMux) g_littlefsMux = xSemaphoreCreateMutex();
  if (!s_listMux) s_listMux = xSemaphoreCreateMutex();
  if (!g_dlnaQueue) g_dlnaQueue = xQueueCreate(6, sizeof(DlnaJob));

  BaseType_t ok = xTaskCreatePinnedToCore(
    dlna_worker_task,
    "dlna_worker",
    24 * 1024,     // 12KB elég (ha kell, később feljebb)
    nullptr,
    2,             // közepes prio
    &s_workerTask,
    0              // !!! CORE0 (nálad core1 halál)
  );

  if (ok != pdPASS) {
    s_workerTask = nullptr;
    Serial.println("[DLNA] worker task create FAILED");
  } else {
    Serial.println("[DLNA] worker task started");
  }
}


bool dlna_worker_enqueue(const DlnaJob& j) {
  if (!g_dlnaQueue) return false;
  return xQueueSend(g_dlnaQueue, &j, 0) == pdTRUE;
}
#endif   // USE_DLNA