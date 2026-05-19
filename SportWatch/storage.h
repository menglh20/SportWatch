#pragma once

#include <Arduino.h>

struct DailyStats {
  uint16_t day_of_year;
  uint32_t running_sec;
  uint32_t walking_sec;
  uint32_t other_sec;
};

bool storage_begin();
bool storage_load_stats(DailyStats* out);
bool storage_save_stats(const DailyStats& s);
bool storage_save_last_time(uint32_t epoch_seconds);
bool storage_load_last_time(uint32_t* out);
