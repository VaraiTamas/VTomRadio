#pragma once

#include <stdint.h>

void clock_tts_setup();
void clock_tts_loop();
void clock_tts_force(const char* text, const char* lang = nullptr);

void clock_tts_set_language(const char* lang);
void clock_tts_set_interval(int minutes);
void clock_tts_enable(bool enable);
void clock_tts_set_only_when_no_stream(bool enable);
void clock_tts_set_quiet_hours(bool enabled, uint16_t fromMinutes, uint16_t toMinutes);