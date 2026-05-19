#pragma once

#include <Arduino.h>
#include <time.h>

enum Screen {
  SCREEN_WATCHFACE,
  SCREEN_STATS,
};

bool display_begin();
void display_boot_message(const char* line1, const char* line2);
void display_render(Screen screen,
                    const struct tm& local_time,
                    float ambient_temp_c,
                    uint32_t running_sec,
                    uint32_t walking_sec,
                    uint32_t other_sec,
                    bool time_synced);
