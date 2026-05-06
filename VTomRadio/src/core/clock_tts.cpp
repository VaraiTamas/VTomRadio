// Módosítva, átírva hibák javítva.
#include "clock_tts.h"
#include "options.h"
#include "../../myoptions.h"
#include "config.h"
#include "network.h"
#include "player.h"
#include <Arduino.h>
#include <time.h>

static int clock_tts_prev_volume = 0;
static bool clock_tts_fading_down = false;
static bool clock_tts_fading_up = false;
static unsigned long clock_tts_fade_timer = 0;
static int clock_tts_fade_volume = -1;
static unsigned long clock_lastTTSMillis = 0;
static bool clock_ttsActive = false;
static unsigned long clock_tts_started_at = 0;
static unsigned long clock_tts_last_progress_at = 0;
static uint32_t clock_tts_last_audio_time = 0;
static bool clock_tts_audio_progress_seen = false;
static int clock_lastMinute = -1;
static int clock_tts_saved_station = -1;
static bool clock_tts_resume_after = false;
static constexpr unsigned long CLOCK_TTS_MAX_ACTIVE_MS = 12000;
static constexpr unsigned long CLOCK_TTS_NO_PROGRESS_MS = 3500;

// Konfigurációs változók
static bool clock_tts_enabled = false;
static int clock_tts_interval = 60;
static char clock_tts_language[8] = "HU";
static bool clock_tts_only_when_no_stream = false;
static bool clock_tts_quiet_hours_enabled = false;
static uint16_t clock_tts_quiet_from_minutes = 23 * 60;
static uint16_t clock_tts_quiet_to_minutes = 7 * 60;

static tm clock_tts_get_display_time_snapshot() {
  tm snapshot{};
  network_get_timeinfo_snapshot(&snapshot);
  return snapshot;
}

static bool clock_tts_is_quiet_time(const tm* tm_struct) {
  if (!clock_tts_quiet_hours_enabled || !tm_struct) {
    return false;
  }
  uint16_t nowMin = static_cast<uint16_t>(tm_struct->tm_hour * 60 + tm_struct->tm_min);
  if (clock_tts_quiet_from_minutes == clock_tts_quiet_to_minutes) {
    return false;
  }
  if (clock_tts_quiet_from_minutes < clock_tts_quiet_to_minutes) {
    return nowMin >= clock_tts_quiet_from_minutes && nowMin < clock_tts_quiet_to_minutes;
  }
  return nowMin >= clock_tts_quiet_from_minutes || nowMin < clock_tts_quiet_to_minutes;
}

static bool clock_tts_is_supported_lang(const char* lang) {
  if (!lang) {
    return false;
  }
  return strncmp(lang, "EN", 2) == 0 || strncmp(lang, "HU", 2) == 0 || strncmp(lang, "PL", 2) == 0 || strncmp(lang, "NL", 2) == 0
      || strncmp(lang, "DE", 2) == 0 || strncmp(lang, "RU", 2) == 0 || strncmp(lang, "RO", 2) == 0 || strncmp(lang, "FR", 2) == 0
      || strncmp(lang, "GR", 2) == 0;
}

void clock_tts_set_language(const char* lang) {
  if (!lang || lang[0] == '\0') {
    return;
  }
  clock_tts_language[0] = lang[0];
  if (lang[1] == '\0') {
    clock_tts_language[1] = '\0';
    return;
  }
  clock_tts_language[1] = lang[1];
  clock_tts_language[2] = '\0';
}

void clock_tts_set_interval(int minutes) {
  if (minutes > 0) {
    clock_tts_interval = minutes;
  }
}

void clock_tts_set_only_when_no_stream(bool enable) {
  clock_tts_only_when_no_stream = enable;
}

void clock_tts_set_quiet_hours(bool enabled, uint16_t fromMinutes, uint16_t toMinutes) {
  clock_tts_quiet_hours_enabled = enabled;
  clock_tts_quiet_from_minutes = fromMinutes % (24 * 60);
  clock_tts_quiet_to_minutes = toMinutes % (24 * 60);
}

void clock_tts_enable(bool enable) {
  clock_tts_enabled = enable;
  if (!enable) {
    if (clock_tts_prev_volume > 0) {
      player.setVolume(clock_tts_prev_volume);
    }
    clock_tts_fading_down = false;
    clock_tts_fading_up = false;
    clock_tts_fade_volume = -1;
    clock_ttsActive = false;
    clock_tts_started_at = 0;
    clock_tts_last_progress_at = 0;
    clock_tts_last_audio_time = 0;
    clock_tts_audio_progress_seen = false;
    clock_tts_resume_after = false;
    config.isClockTTS = false;
  }
}

void clock_tts_setup() {
  clock_tts_prev_volume = 0;
  clock_tts_fading_down = false;
  clock_tts_fading_up = false;
  clock_tts_fade_timer = 0;
  clock_tts_fade_volume = -1;
  clock_lastTTSMillis = 0;
  clock_ttsActive = false;
  clock_tts_started_at = 0;
  clock_tts_last_progress_at = 0;
  clock_tts_last_audio_time = 0;
  clock_tts_audio_progress_seen = false;
  clock_tts_resume_after = false;
  clock_lastMinute = -1;
  clock_tts_enabled = config.store.clockTtsEnabled;
  clock_tts_set_interval(config.store.clockTtsIntervalMinutes);
  clock_tts_set_language(config.store.clockTtsLanguage);
  clock_tts_set_only_when_no_stream(config.store.clockTtsOnlyWhenNoStream);
  clock_tts_set_quiet_hours(config.store.clockTtsQuietHoursEnabled, config.store.clockTtsQuietFromMinutes, config.store.clockTtsQuietToMinutes);
  if (!clock_tts_is_supported_lang(clock_tts_language)) {
    clock_tts_set_language("HU");
  }
}

void clock_tts_force(const char* text, const char* lang) {
  if (player.connecttospeech(text, lang ? lang : clock_tts_language)) {
    clock_lastTTSMillis = millis();
    clock_tts_started_at = clock_lastTTSMillis;
    clock_tts_last_progress_at = clock_lastTTSMillis;
    clock_tts_last_audio_time = player.getAudioCurrentTime();
    clock_tts_audio_progress_seen = false;
    clock_tts_resume_after = false;
    clock_ttsActive = true;
    config.isClockTTS = true;
  }
}

static void clock_tts_announcement(char* buf, size_t buflen, int hour, int min, const char* lang) {
  if (strncmp(lang, "PL", 2) == 0) {
    snprintf(buf, buflen, "Jest godzina %d:%02d.", hour, min);
  } else if (strncmp(lang, "HU", 2) == 0) {
    snprintf(buf, buflen, "Az idő %d:%02d.", hour, min);
  } else if (strncmp(lang, "RU", 2) == 0) {
    snprintf(buf, buflen, "Сейчас %d:%02d.", hour, min);
  } else if (strncmp(lang, "DE", 2) == 0) {
    snprintf(buf, buflen, "Es ist %d Uhr %02d.", hour, min);
  } else if (strncmp(lang, "FR", 2) == 0) {
    snprintf(buf, buflen, "Il est %d:%02d.", hour, min);
  } else if (strncmp(lang, "GR", 2) == 0) {
    snprintf(buf, buflen, "I ora einai %d:%02d.", hour, min);
  } else if (strncmp(lang, "RO", 2) == 0) {
    snprintf(buf, buflen, "Este ora %d:%02d.", hour, min);
  } else if (strncmp(lang, "NL", 2) == 0) {
    snprintf(buf, buflen, "De tijd %d:%02d.", hour, min);
  } else {
    snprintf(buf, buflen, "The time is %d:%02d.", hour, min);
  }
}

void clock_tts_loop() {
  if (!clock_tts_enabled) {
    return;
  }
  if (config.getMode() == PM_SDCARD) {
    return;
  }
  unsigned long nowMillis = millis();
  tm localTm = clock_tts_get_display_time_snapshot();
  struct tm* tm_struct = &localTm;

    // --- Fokozatos elhalkulás a TTS előtt ---
  if (clock_tts_fading_down) {
    if (clock_tts_fade_volume == -1) {
      clock_tts_fade_volume = player.getVolume();
      clock_tts_prev_volume = clock_tts_fade_volume;
    }
    if (nowMillis - clock_tts_fade_timer > 50 && clock_tts_fade_volume > 0) {
      clock_tts_fade_volume -= 1;
      if (clock_tts_fade_volume < 0) {
        clock_tts_fade_volume = 0;
      }
      player.setVolume(clock_tts_fade_volume);
      clock_tts_fade_timer = nowMillis;
    }
    if (clock_tts_fade_volume <= 0) {
      clock_tts_saved_station = config.lastStation();  // Aktuális állomás mentése
      config.isClockTTS = true;
      delay(150);
      char buf[48];
      clock_tts_announcement(buf, sizeof(buf), tm_struct->tm_hour, tm_struct->tm_min, clock_tts_language);
      player.setVolume(clock_tts_prev_volume);
      if (player.connecttospeech(buf, clock_tts_language)) {
        clock_lastTTSMillis = nowMillis;
        clock_tts_started_at = nowMillis;
        clock_tts_last_progress_at = nowMillis;
        clock_tts_last_audio_time = player.getAudioCurrentTime();
        clock_tts_audio_progress_seen = false;
        clock_tts_resume_after = true;
        clock_ttsActive = true;
      } else {
        // TTS request failed, immediately recover stream state.
        player.sendCommand({PR_PLAY, clock_tts_saved_station});
        clock_tts_fading_up = true;
        clock_tts_fade_timer = nowMillis;
        clock_tts_started_at = 0;
        clock_tts_last_progress_at = 0;
        clock_tts_last_audio_time = 0;
        clock_tts_audio_progress_seen = false;
        clock_tts_resume_after = false;
        clock_ttsActive = false;
        config.isClockTTS = false;
      }
      clock_tts_fading_down = false;
      clock_lastMinute = tm_struct->tm_min;
      clock_tts_fade_volume = -1;
    }
    return;
  }

  // --- Fokozatos hangerőnövekedés a TTS után ---
  if (clock_tts_fading_up) {
    if (clock_tts_fade_volume == -1) {
      clock_tts_fade_volume = 0;
    }
    if (nowMillis - clock_tts_fade_timer > 80 && clock_tts_fade_volume < clock_tts_prev_volume) {
      clock_tts_fade_volume += 1;
      player.setVolume(clock_tts_fade_volume);
      clock_tts_fade_timer = nowMillis;
    }
    if (clock_tts_fade_volume >= clock_tts_prev_volume) {
      player.setVolume(clock_tts_prev_volume);
      clock_tts_fading_up = false;
      clock_tts_fade_volume = -1;
    }
    return;
  }

  if (tm_struct->tm_year + 1900 < 2020) {
    return;
  }
  if (tm_struct->tm_min % clock_tts_interval == 0 && tm_struct->tm_min != clock_lastMinute && tm_struct->tm_sec < 2 && !clock_ttsActive) {
    if (!clock_tts_is_quiet_time(tm_struct)) {
      if (clock_tts_only_when_no_stream) {
        if (!player.isRunning()) {
          char buf[48];
          clock_tts_announcement(buf, sizeof(buf), tm_struct->tm_hour, tm_struct->tm_min, clock_tts_language);
          if (player.connecttospeech(buf, clock_tts_language)) {
            clock_lastTTSMillis = nowMillis;
            clock_tts_started_at = nowMillis;
            clock_tts_last_progress_at = nowMillis;
            clock_tts_last_audio_time = player.getAudioCurrentTime();
            clock_tts_audio_progress_seen = false;
            clock_tts_resume_after = false;
            clock_ttsActive = true;
            config.isClockTTS = true;
          }
          clock_lastMinute = tm_struct->tm_min;
        }
      } else if (player.isRunning()) {
        clock_tts_fading_down = true;  // Engedélyezi a halkítást.
        clock_tts_fade_timer = nowMillis;
        return;
      }
    }
  }
  if (clock_ttsActive) {
    uint32_t nowAudioTime = player.getAudioCurrentTime();
    if (nowAudioTime != clock_tts_last_audio_time || player.inBufferFilled() > 0) {
      clock_tts_last_audio_time = nowAudioTime;
      clock_tts_last_progress_at = nowMillis;
      clock_tts_audio_progress_seen = true;
    }

    bool shouldRecover = false;
    if (!player.isRunning() || (nowMillis - clock_tts_started_at > CLOCK_TTS_MAX_ACTIVE_MS)) {
      shouldRecover = true;
    }
    if (!clock_tts_audio_progress_seen && (nowMillis - clock_tts_started_at > CLOCK_TTS_NO_PROGRESS_MS)) {
      shouldRecover = true;
    }

    if (!shouldRecover) {
      return;
    }

    if (clock_tts_resume_after) {
      player.sendCommand({PR_PLAY, clock_tts_saved_station});
      clock_tts_fading_up = true;
      clock_tts_fade_timer = nowMillis;
    }
    clock_tts_started_at = 0;
    clock_tts_last_progress_at = 0;
    clock_tts_last_audio_time = 0;
    clock_tts_audio_progress_seen = false;
    clock_tts_resume_after = false;
    clock_ttsActive = false;
    config.isClockTTS = false;
    return;
  }
}