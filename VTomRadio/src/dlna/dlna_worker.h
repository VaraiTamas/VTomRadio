#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

enum DlnaJobType : uint8_t { DJ_INIT=0, DJ_BUILD=1, DJ_APPEND=2, DJ_CANCEL=3, DJ_LIST=4, DJ_BUILD_SEL=5, DJ_APPEND_SEL=6 };

struct DlnaJob {
  DlnaJobType type;
  char objectId[64];   // DLNA objectId
//  char key[32];        // opcionális: category/genre key
  uint32_t reqId;      // monoton növekvő request id
  uint32_t hardLimit;
  uint32_t start;      // DJ_LIST: lapozási kezdőindex
};

struct DlnaStatus {
  volatile bool busy;
  volatile bool ok;
  volatile int  err;           // 0=OK, HTTP-szerű / belső hibakód
  volatile uint32_t reqId;
  volatile uint32_t playlistVer;   // fájlverzió a preview sync-hez
  char msg[96];
  volatile uint32_t scanNext;   // 423: innen folytatható a keresés
  volatile uint32_t scanTotal;
};

extern QueueHandle_t g_dlnaQueue;
extern SemaphoreHandle_t g_littlefsMux;
extern DlnaStatus g_dlnaStatus;

void dlna_worker_start();
bool dlna_worker_enqueue(const DlnaJob& j);

// Aszinkron lista: a böngészés a worker taskban fut, a web szerver szál nem blokkol
bool dlna_list_submit(const char* objectId, uint32_t start, uint32_t reqId);
// 0 = folyamatban, 1 = kész (out kitöltve), -1 = hiba, -2 = ismeretlen reqId
int  dlna_list_result(uint32_t reqId, String& out);
// POST /dlna/sel törzs: soronként "title\turl"; a job (DJ_BUILD_SEL / DJ_APPEND_SEL) ezt dolgozza fel
void dlna_sel_chunk(const uint8_t* data, size_t len, size_t index);
bool dlna_sel_overflow();
bool dlna_sel_empty();
void dlna_status_setBusy(const DlnaJob& j, const char* msg);
void dlna_status_setDone(const DlnaJob& j, bool ok, int err, const char* msg);
void dlna_status_progress(const char* msg);   // csak busy közben írja át az üzenetet
uint32_t dlna_playlist_version();
