#include "storage.h"
#include <Preferences.h>

static Preferences prefs;
static const char* NS = "sportwatch";

bool storage_begin() {
  if (!prefs.begin(NS, false)) {
    Serial.println("[storage] NVS init failed");
    return false;
  }
  Serial.println("[storage] NVS ready");
  return true;
}

bool storage_load_stats(DailyStats* out) {
  if (!out) return false;
  out->day_of_year = prefs.getUShort("day", 0xFFFF);
  out->running_sec = prefs.getULong("run", 0);
  out->walking_sec = prefs.getULong("walk", 0);
  out->other_sec   = prefs.getULong("other", 0);
  return true;
}

bool storage_save_stats(const DailyStats& s) {
  prefs.putUShort("day", s.day_of_year);
  prefs.putULong("run",   s.running_sec);
  prefs.putULong("walk",  s.walking_sec);
  prefs.putULong("other", s.other_sec);
  return true;
}

bool storage_save_last_time(uint32_t epoch_seconds) {
  return prefs.putULong("lastT", epoch_seconds) > 0;
}

bool storage_load_last_time(uint32_t* out) {
  if (!out) return false;
  *out = prefs.getULong("lastT", 0);
  return *out != 0;
}
