#include <Arduino.h>
#include <Wire.h>
#include <time.h>

#include "config.h"
#include "sensors.h"
#include "display.h"
#include "timekeeper.h"
#include "storage.h"
#include "activity.h"

static DailyStats g_stats = {};
static Screen g_screen = SCREEN_WATCHFACE;
static uint32_t g_last_sample = 0;
static uint32_t g_last_infer = 0;
static uint32_t g_last_display = 0;
static uint32_t g_last_switch = 0;
static uint32_t g_last_save = 0;

static void roll_over_day_if_needed(const struct tm& now) {
  uint16_t today = (uint16_t)now.tm_yday;
  if (g_stats.day_of_year != today) {
    g_stats.day_of_year = today;
    g_stats.running_sec = 0;
    g_stats.walking_sec = 0;
    g_stats.other_sec = 0;
    storage_save_stats(g_stats);
    Serial.printf("[main] new day %u, stats reset\n", today);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[main] SportWatch boot");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

  display_begin();
  display_boot_message("SportWatch", "init...");

  sensors_begin();
  storage_begin();
  activity_begin();
  timekeeper_begin();

  storage_load_stats(&g_stats);

  display_boot_message("Sync time", "connect WiFi...");
  timekeeper_sync_at_boot();

  struct tm now;
  if (timekeeper_get_local_time(&now)) {
    roll_over_day_if_needed(now);
  }

  Serial.println("[main] ready");
}

void loop() {
  uint32_t now_ms = millis();

  if (now_ms - g_last_sample >= IMU_SAMPLE_PERIOD_MS) {
    g_last_sample = now_ms;
    ImuSample s;
    if (sensors_read(&s)) {
      activity_push_sample(s);
    }
  }

  if (now_ms - g_last_infer >= INFER_PERIOD_MS) {
    g_last_infer = now_ms;
    Activity a = activity_infer();
    static uint32_t run_ms_extra = 0, walk_ms_extra = 0, other_ms_extra = 0;
    uint32_t* bucket = (a == ACT_RUNNING) ? &run_ms_extra
                     : (a == ACT_WALKING) ? &walk_ms_extra
                                          : &other_ms_extra;
    *bucket += INFER_PERIOD_MS;
    while (*bucket >= 1000) {
      *bucket -= 1000;
      if (a == ACT_RUNNING)      g_stats.running_sec++;
      else if (a == ACT_WALKING) g_stats.walking_sec++;
      else                       g_stats.other_sec++;
    }
  }

  if (now_ms - g_last_display >= DISPLAY_PERIOD_MS) {
    g_last_display = now_ms;
    struct tm tnow;
    bool have_time = timekeeper_get_local_time(&tnow);
    float temp = sensors_ambient_temp_c();
    if (have_time) {
      display_render(g_screen, tnow, temp,
                     g_stats.running_sec, g_stats.walking_sec, g_stats.other_sec,
                     timekeeper_is_synced());
    }
  }

  if (now_ms - g_last_switch >= SCREEN_SWITCH_MS) {
    g_last_switch = now_ms;
    g_screen = (g_screen == SCREEN_WATCHFACE) ? SCREEN_STATS : SCREEN_WATCHFACE;
  }

  if (now_ms - g_last_save >= STATS_SAVE_MS) {
    g_last_save = now_ms;
    storage_save_stats(g_stats);
    time_t epoch = time(nullptr);
    if (epoch > 0) storage_save_last_time((uint32_t)epoch);
  }

  if (timekeeper_is_new_day()) {
    struct tm tnow;
    if (timekeeper_get_local_time(&tnow)) roll_over_day_if_needed(tnow);
  }

  if (timekeeper_should_daily_resync()) {
    timekeeper_resync_now();
  }
}
