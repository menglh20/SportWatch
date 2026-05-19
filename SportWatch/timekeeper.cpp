#include "timekeeper.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <sys/time.h>

static bool g_synced = false;
static int g_last_day = -1;
static int g_last_resync_day = -1;

static void apply_timezone() {
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();
}

static void set_from_compile_time() {
  struct tm tm_init = {};
  if (strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm_init)) {
    time_t t = mktime(&tm_init);
    struct timeval tv = { t, 0 };
    settimeofday(&tv, nullptr);
    Serial.println("[time] using compile-time fallback");
  }
}

static bool ntp_sync_blocking(uint32_t timeout_ms) {
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  apply_timezone();
  uint32_t start = millis();
  struct tm info;
  while (millis() - start < timeout_ms) {
    if (getLocalTime(&info, 100) && info.tm_year > (2024 - 1900)) {
      Serial.printf("[time] NTP ok: %04d-%02d-%02d %02d:%02d:%02d\n",
                    info.tm_year + 1900, info.tm_mon + 1, info.tm_mday,
                    info.tm_hour, info.tm_min, info.tm_sec);
      return true;
    }
    delay(200);
  }
  return false;
}

bool timekeeper_begin() {
  apply_timezone();
  return true;
}

bool timekeeper_sync_at_boot() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);
  Serial.println("[time] attempting WiFi for NTP...");
  if (wm.autoConnect(AP_CONFIG_SSID, AP_CONFIG_PASSWORD)) {
    if (ntp_sync_blocking(WIFI_CONNECT_TIMEOUT_MS)) {
      g_synced = true;
      WiFi.disconnect(true, false);
      WiFi.mode(WIFI_OFF);
      Serial.println("[time] WiFi off; running on internal RTC");
      struct tm now;
      if (timekeeper_get_local_time(&now)) {
        g_last_day = now.tm_yday;
        g_last_resync_day = now.tm_yday;
      }
      return true;
    }
  }
  Serial.println("[time] NTP failed");
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  set_from_compile_time();
  struct tm now;
  if (timekeeper_get_local_time(&now)) g_last_day = now.tm_yday;
  return false;
}

bool timekeeper_resync_now() {
  Serial.println("[time] periodic resync attempt...");
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }
  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    ok = ntp_sync_blocking(WIFI_CONNECT_TIMEOUT_MS);
    if (ok) g_synced = true;
  }
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  struct tm now;
  if (timekeeper_get_local_time(&now)) g_last_resync_day = now.tm_yday;
  Serial.printf("[time] resync %s\n", ok ? "ok" : "failed");
  return ok;
}

bool timekeeper_get_local_time(struct tm* out) {
  if (!out) return false;
  return getLocalTime(out, 50);
}

bool timekeeper_is_synced() { return g_synced; }

bool timekeeper_is_new_day() {
  struct tm now;
  if (!timekeeper_get_local_time(&now)) return false;
  if (g_last_day < 0) { g_last_day = now.tm_yday; return false; }
  if (now.tm_yday != g_last_day) {
    g_last_day = now.tm_yday;
    return true;
  }
  return false;
}

bool timekeeper_should_daily_resync() {
  struct tm now;
  if (!timekeeper_get_local_time(&now)) return false;
  if (now.tm_hour != DAILY_RESYNC_HOUR) return false;
  if (now.tm_yday == g_last_resync_day) return false;
  return true;
}
