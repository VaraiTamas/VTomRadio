#pragma once

#include <Arduino.h>

void serial_littlefs_begin(Stream& serial);
void serial_littlefs_poll();
bool serial_littlefs_is_active();
