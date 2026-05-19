#pragma once

#include <Arduino.h>
#include <time.h>

bool timekeeper_begin();

// Tries: NTP via WiFiManager → compile-time fallback. Returns true if NTP succeeded.
bool timekeeper_sync_at_boot();

// Brief reconnect+NTP, called periodically (e.g., daily). Returns true on success.
bool timekeeper_resync_now();

bool timekeeper_get_local_time(struct tm* out);
bool timekeeper_is_synced();

// True if local time has crossed into a new calendar day since last call.
bool timekeeper_is_new_day();

// True roughly once per day around DAILY_RESYNC_HOUR (used by main loop).
bool timekeeper_should_daily_resync();
