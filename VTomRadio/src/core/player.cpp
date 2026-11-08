#include "options.h"
#include "player.h"
#include "config.h"
#include "display.h"
#include "sdmanager.h"
#include "netserver.h"
#include "timekeeper.h"
#include "../displays/tools/language.h"
#include "../pluginsManager/pluginsManager.h"
namespace {
constexpr uint8_t VOLUME_CURVE_POINTS = 22;
constexpr float   VOLUME_CURVE_MIN_DB = -60.0f;
constexpr float   VOLUME_CURVE_MAX_DB = 0.0f;

// Default 22-point user curve for volume steps 0..21 in dB.
// Edit these values (or set them at runtime) to tailor perceived loudness.
float g_volumeCurveDbLut[VOLUME_CURVE_POINTS] = {
  -60.0f, -52.0f, -39.0f, -32.0f, -27.0f, -24.0f, -20.0f, -18.0f, -15.0f, -13.0f, -12.0f,
  -10.0f, -9.0f, -8.0f, -6.0f, -5.0f, -4.0f, -4.0f, -3.0f, -2.0f, -2.0f, -1.0f};

float clampCurveDb(float db) {
  if (db < VOLUME_CURVE_MIN_DB) return VOLUME_CURVE_MIN_DB;
  if (db > VOLUME_CURVE_MAX_DB) return VOLUME_CURVE_MAX_DB;
  return db;
}

void clampCurveLut(float* lut, size_t n) {
  if (!lut || n == 0) return;
  for (size_t i = 0; i < n; ++i) {
    lut[i] = clampCurveDb(lut[i]);
  }
}

float playerVolumeCurveDb(float t) {
  if (t <= 0.0f) return g_volumeCurveDbLut[0];
  if (t >= 1.0f) return g_volumeCurveDbLut[VOLUME_CURVE_POINTS - 1];

  float pos = t * (float)(VOLUME_CURVE_POINTS - 1);
  uint8_t i = (uint8_t)pos;
  if (i >= VOLUME_CURVE_POINTS - 1) return g_volumeCurveDbLut[VOLUME_CURVE_POINTS - 1];
  float frac = pos - (float)i;
  float a = g_volumeCurveDbLut[i];
  float b = g_volumeCurveDbLut[i + 1];
  return a + (b - a) * frac;
}
} // namespace

Player player;
QueueHandle_t playerQueue;

  #if !I2S_INTERNAL
Player::Player() {}
  #else
Player::Player() : Audio(true, I2S_DAC_CHANNEL_BOTH_EN) {}
  #endif
//#endif

void Player::init() {
  Serial.print("##[BOOT]#\tplayer.init\t");
  playerQueue = NULL;
  _resumeFilePos = 0;
  _hasError = false;
  playerQueue = xQueueCreate(5, sizeof(playerRequestParams_t));
  setOutputPins(false);
  delay(50);
#ifdef MQTT_ROOT_TOPIC
  memset(burl, 0, MQTT_BURL_SIZE);
#endif
  if (MUTE_PIN != 255) {
    pinMode(MUTE_PIN, OUTPUT);
  }
#if I2S_DOUT != 255
  #if !I2S_INTERNAL
  setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  #endif
#endif
  setBalance(config.store.balance);
  setTone(config.store.bass, config.store.middle, config.store.trebble);
  setVolumeSteps(21);  // Alapértelmezetten 21, ami 0-21-ig terjedő értékeket engedélyez. Ez a változtatás lehetővé teszi a finomabb hangerőszabályozást, különösen alacsonyabb hangerőszinteken.
  for (uint8_t i = 1; i <= 21; ++i) {
    int db = (int)config.store.volumeCurveDb[i - 1];
    if (db < -60) db = -60;
    if (db > 0) db = 0;
    g_volumeCurveDbLut[i] = (float)db;
  }
  clampCurveLut(g_volumeCurveDbLut, VOLUME_CURVE_POINTS);
  setVolumeCurve(playerVolumeCurveDb);
  setVolume(0, 0);
  _status = STOPPED;
  _volTimer = false;
//randomSeed(analogRead(0));
#if PLAYER_FORCE_MONO
  forceMono(true);
#endif
  _loadVol();
  setConnectionTimeout(CONNECTION_TIMEOUT, CONNECTION_TIMEOUT_SSL);
  Serial.println("done");
}

bool Player::sendCommand(playerRequestParams_t request) {
  if (playerQueue == NULL) {
    return false;
  }
  return xQueueSend(playerQueue, &request, PLQ_SEND_DELAY) == pdPASS;
}

bool Player::sendCommandFirst(playerRequestParams_t request) {
  if (playerQueue == NULL) {
    return false;
  }
  return xQueueSendToFront(playerQueue, &request, PLQ_SEND_DELAY) == pdPASS;
}

void Player::resetQueue() {
  if (playerQueue != NULL) {
    xQueueReset(playerQueue);
  }
}

void Player::stopInfo() {
  netserver.requestOnChange(MODE, 0);
}

void Player::setError() {
  _hasError = true;
  config.setTitle(config.tmpBuf);
  Serial.printf("##ERROR#:\t%s\r\n", config.tmpBuf);
}

void Player::setError(const char *e) {
  strlcpy(config.tmpBuf, e, sizeof(config.tmpBuf));
  setError();
}

/* Ha az alreadyStopped true akkor a STOP művelet már le lett kezelve.*/
void Player::_stop(bool alreadyStopped) {
  log_i("%s called", __func__);
  if (config.getMode() == PM_SDCARD && !alreadyStopped) {
    config.sdResumePos = player.getAudioFilePosition();
    config.stopedSdStationId = config.lastStation();
  }
  _status = STOPPED;
  setOutputPins(false);
  if (!_hasError) {
    config.setTitle((display.mode() == LOST || display.mode() == UPDATING) ? "" : LANG::const_PlStopped);
  }
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNKNOWN);
  if (!alreadyStopped) {
    stopSong();
  }
  netserver.requestOnChange(BITRATE, 0);
  display.putRequest(DBITRATE);
  display.putRequest(PSTOP);
  if (!lockOutput) {
    stopInfo();
  }
  if (player_on_stop_play) {
    player_on_stop_play();
  }
  pm.on_stop_play();
}

void Player::initHeaders(const char *file) {
  if (strlen(file) == 0 || true) {
    return;  //TODO Read TAGs
  }
  connecttoFS(sdman, file);
  //eofHeader = false; // megszűnt az Audio.h ban.
  //while(!eofHeader) Audio::loop(); // megszűnt az Audio.h ban.
  //netserver.requestOnChange(SDPOS, 0);
  //setDefaults(); // free buffers and set defaults átkerült a privat függvényekhez az Audio.h ban.
}
void resetPlayer() {
  if (!config.store.watchdog) {
    return;
  }
  player.resetQueue();
  player.sendCommand({PR_STOP, 0});
  player.loop();
}

#ifndef PL_QUEUE_TICKS
  #define PL_QUEUE_TICKS 0
#endif
#ifndef PL_QUEUE_TICKS_ST
  #define PL_QUEUE_TICKS_ST 15
#endif
void Player::loop() {
  if (playerQueue == NULL) {
    return;
  }
  playerRequestParams_t requestP;
  if (xQueueReceive(playerQueue, &requestP, isRunning() ? PL_QUEUE_TICKS : PL_QUEUE_TICKS_ST)) {
    switch (requestP.type) {
      case PR_STOP: _stop(); break;
      case PR_PLAY:
      {
        uint16_t st = (uint16_t)abs(requestP.payload);

#ifdef USE_DLNA
        if (config.store.playlistSource == PL_SRC_DLNA) {
          config.store.lastDlnaStation = st;
          config.saveValue(&config.store.lastDlnaStation, (uint16_t)st);
          config.sdResumePos = 0;
          config.saveValue(&config.store.lastPlayedSource, (uint8_t)PL_SRC_DLNA);
          _play(st);
          netserver.requestOnChange(GETINDEX, 0);
          break;
        }
#endif

        // ==== EREDETI VISSELKEDÉS (WEB / SD) ====
        if (requestP.payload > 0) {
          config.setLastStation(st);
        }
#ifdef USE_DLNA
        config.saveValue(&config.store.lastPlayedSource, (uint8_t)PL_SRC_WEB);
#endif
        _play(st);
        if (player_on_station_change) {
          player_on_station_change();
        }
        pm.on_station_change();
        break;
      }
      case PR_TOGGLE:
      {
        toggle();
        break;
      }
      case PR_VOL:
      {
        config.setVolume(requestP.payload);
        Audio::setVolume(volToI2S(requestP.payload), 0);
        break;
      }
      #ifdef USE_SD
      case PR_CHECKSD:
      {
        if (config.getMode() == PM_SDCARD) {
          if (!sdman.cardPresent()) {
            // A driver "Card Failed" után tartósan NOINIT állapotba teszi a kártyát, ezért újrainicializáljuk.
            Serial.println("[SD] card check failed, reinitializing");
            sdman.stop();
            vTaskDelay(50);
            bool ok = false;
            for (int i = 0; i < 3 && !ok; i++) {
              ok = sdman.start() && sdman.cardPresent();
              if (!ok) { sdman.stop(); vTaskDelay(100); }
            }
            if (ok) {
              sendCommand({PR_PLAY, config.lastStation()});  // az aktuális szám újraindítása
            } else {
              config.changeMode(PM_WEB);
            }
          }
        }
        break;
      }
#endif
      case PR_VUTONUS:  // 2 másodpercenként hívja a timekeeper.
      {
        if (config.vuRefLevel > 10) {
          config.vuRefLevel -= 10;  // A tárolt VU csúcs csökkentése, hogy ne ragadjon fenn.
        }
        break;
      }
      case PR_BURL:
      {
#ifdef MQTT_ROOT_TOPIC
        if (strlen(burl) > 0) {
          browseUrl();
        }
#endif
        break;
      }
      case PR_URL:
      {
        // Play arbitrary URL (presets)
        _hasError = false;
        remoteStationName = false;
        config.setDspOn(1);

        if (_status == PLAYING) {
          _stop();
        }

        // Set station label for UI
        if (_nameBuf[0] != '\0') {
          strlcpy(config.station.name, _nameBuf, sizeof(config.station.name));
        } else {
          strlcpy(config.station.name, "URL", sizeof(config.station.name));
        }
        strlcpy(config.station.url, _urlBuf, sizeof(config.station.url));
        config.station.title[0] = '\0';

        // Always WEB mode
        config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));

        display.putRequest(PSTOP);
        setOutputPins(false);
        config.setTitle(LANG::const_PlConnect);

        if (connecttohost(config.station.url)) {
          _status = PLAYING;
          config.setTitle("");
          netserver.requestOnChange(MODE, 0);
          setOutputPins(true);
          display.putRequest(PSTART);
          if (player_on_start_play) {
            player_on_start_play();
          }
          pm.on_start_play();
        } else {
          Serial.printf("##ERROR#:	Error connecting to %.128s\n", config.station.url);
          snprintf(config.tmpBuf, sizeof(config.tmpBuf), "Error connecting to %.128s", config.station.url);
          setError();
          _stop(true);
        }
        break;
      }
      default: break;
    }
  }
  Audio::loop();

#ifdef USE_SD
if (
  config.getMode() == PM_SDCARD &&
  !isRunning() &&
  _status == PLAYING &&
  player.getAudioFilePosition() == 0
) {
  Serial.println("[SD] EOF -> next()");
  next();
  return;
}
#endif

  if (_volTimer) {
    if ((millis() - _volTicks) > 3000) {
      config.saveVolume();
      _volTimer = false;
    }
  }
  /*
#ifdef MQTT_ROOT_TOPIC
  if(strlen(burl)>0){
    browseUrl();
  }
#endif*/
}

void Player::setOutputPins(bool isPlaying) {
  if (REAL_LEDBUILTIN != 255) {
    digitalWrite(REAL_LEDBUILTIN, LED_INVERT ? !isPlaying : isPlaying);
  }
  bool _ml = MUTE_LOCK ? !MUTE_VAL : (isPlaying ? !MUTE_VAL : MUTE_VAL);
  if (MUTE_PIN != 255) {
    digitalWrite(MUTE_PIN, _ml);
  }
}

void Player::playUrl(const char *url, const char *name) {
  if (!url || url[0] == '\0') {
    return;
  }
  // Copy into buffers (used by PR_URL in player task)
  strlcpy(_urlBuf, url, sizeof(_urlBuf));
  if (name && name[0] != '\0') {
    strlcpy(_nameBuf, name, sizeof(_nameBuf));
  } else {
    _nameBuf[0] = '\0';
  }
  sendCommand({PR_URL, 0});
}

void Player::_play(uint16_t stationId) {
  log_i("%s called, stationId=%d", __func__, stationId);
  _hasError = false;
  _status = STOPPED;
  setOutputPins(false);
  remoteStationName = false;
  // Kijelző + metaadat alaphelyzet
  if (!config.prepareForPlaying(stationId)) {
    return;
  }
  _loadVol();
  bool isConnected = false;
  // ----- SD MODE -----
  if (config.getMode() == PM_SDCARD && SDC_CS != 255) {
    // A connecttoFS NEM támogat start offsetet SD-n → -1, indítás pozícionálás nélkül.
    isConnected = connecttoFS(sdman, config.station.url, -1);
    if (!isConnected) {
      // Elveszett SPI szinkron (pl. no token / crc error): kártya újrainicializálása és egy újrapróba.
      Serial.println("[SD] open failed, reinitializing card");
      sdman.stop();
      vTaskDelay(50);
      if (sdman.start()) {
        isConnected = connecttoFS(sdman, config.station.url, -1);
      }
    }
  } else {
#ifdef USE_DLNA //DLNA mod
  // DLNA is WEB engine, de nem írjuk felül a mode-ot
    if (config.store.playlistSource != PL_SRC_DLNA)
#endif
    {
      config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
    }
  }
  if (config.getMode() == PM_WEB) {
    isConnected = connecttohost(config.station.url);
  }
  connproc = true;
  // ----- START PLAYING -----
  if (isConnected) {
    _status = PLAYING;
    config.configPostPlaying(stationId);
    setOutputPins(true);
    if (player_on_start_play) player_on_start_play();
    pm.on_start_play();
  } else {
    Serial.printf("##ERROR#:\tError connecting to %.128s\n", config.station.url);
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "Error connecting to %.128s", config.station.url);
    setError();
    _stop(true);
  }
}

#ifdef MQTT_ROOT_TOPIC
void Player::browseUrl() {
  _hasError = false;
  remoteStationName = true;
  config.setDspOn(1);
  resumeAfterUrl = _status == PLAYING;
  display.putRequest(PSTOP);
  setOutputPins(false);
  config.setTitle(LANG::const_PlConnect);
  if (connecttohost(burl)) {
    _status = PLAYING;
    config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(PSTART);
    if (player_on_start_play) {
      player_on_start_play();
    }
    pm.on_start_play();
  } else {
    Serial.printf("##ERROR#:\tError connecting to %.128s\r\n", burl);
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "Error connecting to %.128s", burl);
    setError();
    _stop(true);
  }
  //memset(burl, 0, MQTT_BURL_SIZE);
}
#endif

void Player::prev() {
  uint16_t lastStation = config.lastStation();
  if (config.getMode() != PM_SDCARD) {
    // WEB + DLNA: always sequential wrap
    if (lastStation <= 1) config.lastStation(config.playlistLength());
    else config.lastStation(lastStation - 1);
  } else {
    // SD: sequential unless snuffle enabled (SD-only feature)
    if (!config.store.sdsnuffle) {
      if (lastStation <= 1) config.lastStation(config.playlistLength());
      else config.lastStation(lastStation - 1);
    } else {
      config.lastStation(random(1, config.playlistLength() + 1));
    }
  }
  config.stopedSdStationId = -1;  // Reseteli a seek hez mentett SD fájl sorszámát.
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::next() {
  uint16_t lastStation = config.lastStation();
  if (config.getMode() != PM_SDCARD) {
    // WEB + DLNA: always sequential wrap
    if (lastStation >= config.playlistLength()) config.lastStation(1);
    else config.lastStation(lastStation + 1);
  } else {
    // SD: sequential unless snuffle enabled
    if (!config.store.sdsnuffle) {
      if (lastStation >= config.playlistLength()) config.lastStation(1);
      else config.lastStation(lastStation + 1);
    } else {
      config.lastStation(random(1, config.playlistLength() + 1));
    }
  }
  config.stopedSdStationId = -1;  // Reseteli a seek hez mentett SD fájl sorszámát.
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::toggle() {
  if (_status == PLAYING) {
    sendCommand({PR_STOP, 0});
  } else {
    sendCommand({PR_PLAY, config.lastStation()});
  }
}

void Player::stepVol(bool up) {
  if (up) {
    if (config.store.volume <= 20) {
      setVol(config.store.volume + 1);
    } else {
      setVol(21);
    }
  } else {
    if (config.store.volume >= 1) {
      setVol(config.store.volume - 1);
    } else {
      setVol(0);
    }
  }
}

/* Ez a fügvény hívódik meg utoljára mielőtt a hangerőt átadnánk az audio osztálynak.
   Zeneszámonként a WEB UI on beállított OVOL hangerő korrekció érték beszámítása a hangerőbe és a 0 - 21 tartományba kényszerítés. */
uint8_t Player::volToI2S(uint8_t volume) {
  int vol = map(volume, 0, 21 - config.station.ovol, 0, 21); 
  if (vol > 21) {
    vol = 21;
  }
  if (vol < 0) {
    vol = 0;
  }
  return vol;
}

void Player::setVolumeCurveDbLut(const float *dbValues, size_t count) {
  if (!dbValues || count != VOLUME_CURVE_POINTS) return;
  for (size_t i = 0; i < VOLUME_CURVE_POINTS; ++i) {
    g_volumeCurveDbLut[i] = dbValues[i];
  }
  clampCurveLut(g_volumeCurveDbLut, VOLUME_CURVE_POINTS);
  setVolumeCurve(playerVolumeCurveDb);
}

void Player::setVolumeCurveDbPoint(uint8_t index, float dbValue) {
  if (index >= VOLUME_CURVE_POINTS) return;
  g_volumeCurveDbLut[index] = dbValue;
  clampCurveLut(g_volumeCurveDbLut, VOLUME_CURVE_POINTS);
  setVolumeCurve(playerVolumeCurveDb);
}

float Player::getVolumeCurveDbPoint(uint8_t index) const {
  if (index >= VOLUME_CURVE_POINTS) return 0.0f;
  return g_volumeCurveDbLut[index];
}

void Player::resetVolumeCurveDbLut() {
  static const float kDefaultVolumeCurve[VOLUME_CURVE_POINTS] = {
      -60.0f, -52.0f, -39.0f, -32.0f, -27.0f, -24.0f, -20.0f, -18.0f, -15.0f, -13.0f, -12.0f,
      -10.0f, -9.0f, -8.0f, -6.0f, -5.0f, -4.0f, -4.0f, -3.0f, -2.0f, -2.0f, -1.0f};
  setVolumeCurveDbLut(kDefaultVolumeCurve, VOLUME_CURVE_POINTS);
}

void Player::_loadVol() {
  setVolume(volToI2S(config.store.volume), 0);
}

void Player::setVol(uint8_t volume) {
  _volTicks = millis();
  _volTimer = true;
  player.sendCommand({PR_VOL, volume});
}

// A WEB UI a hangszínt -16 - +16 között adja, de az Audio osztály setTone()
// függvénye -40 és +6 (dB) közötti értéket kér, ezért mepelni kell.
int8_t Player::uiToDb(int8_t uiVal) {
  if (uiVal == 0) {
    return 0;
  }
  if (uiVal > 0) {
    // 0..+16  →  0..+6 dB
    float db = (uiVal / 16.0f) * 6.0f;
    return (int8_t)roundf(db);
  } else {
    // -16..0  →  -20..0 dB
    float db = (uiVal / 16.0f) * 20.0f;  // uiVal negatív!
    return (int8_t)roundf(db);
  }
}

void Player::setTone(int8_t bass, int8_t mid, int8_t treble) {
  // Serial.printf("EQ UI: %d %d %d  →  DSP: %d %d %d\n", bass, mid, treble, uiToDb(bass), uiToDb(mid), uiToDb(treble));
  Audio::setTone(uiToDb(bass), uiToDb(mid), uiToDb(treble));
}
