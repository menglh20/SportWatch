#include "display.h"
#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

static Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static void format_hms(uint32_t total_sec, char* out, size_t n) {
  uint32_t h = total_sec / 3600;
  uint32_t m = (total_sec % 3600) / 60;
  uint32_t s = total_sec % 60;
  snprintf(out, n, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

bool display_begin() {
  if (!oled.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR)) {
    Serial.println("[display] SSD1306 not found");
    return false;
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.display();
  Serial.println("[display] SSD1306 ready");
  return true;
}

void display_boot_message(const char* line1, const char* line2) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 8);
  if (line1) oled.println(line1);
  oled.setCursor(0, 24);
  if (line2) oled.println(line2);
  oled.display();
}

static void render_watchface(const struct tm& t, float temp_c, bool time_synced) {
  oled.clearDisplay();

  char time_buf[16];
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d", t.tm_hour, t.tm_min);
  oled.setTextSize(3);
  oled.setCursor(8, 8);
  oled.print(time_buf);

  oled.setTextSize(1);
  oled.setCursor(96, 16);
  char sec_buf[8];
  snprintf(sec_buf, sizeof(sec_buf), ":%02d", t.tm_sec);
  oled.print(sec_buf);

  char date_buf[24];
  snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  oled.setCursor(0, 44);
  oled.print(date_buf);

  oled.setCursor(78, 44);
  if (isnan(temp_c)) {
    oled.print("--.- C");
  } else {
    char tbuf[12];
    snprintf(tbuf, sizeof(tbuf), "%4.1f C", temp_c);
    oled.print(tbuf);
  }

  if (!time_synced) {
    oled.setCursor(118, 0);
    oled.print("!");
  }

  oled.display();
}

static void render_stats(uint32_t running_sec, uint32_t walking_sec, uint32_t other_sec) {
  oled.clearDisplay();
  oled.setTextSize(1);

  oled.setCursor(0, 0);
  oled.print("Today");

  char buf[16];

  oled.setCursor(0, 16);
  oled.print("Run :");
  format_hms(running_sec, buf, sizeof(buf));
  oled.setCursor(40, 16);
  oled.print(buf);

  oled.setCursor(0, 30);
  oled.print("Walk:");
  format_hms(walking_sec, buf, sizeof(buf));
  oled.setCursor(40, 30);
  oled.print(buf);

  oled.setCursor(0, 44);
  oled.print("Othr:");
  format_hms(other_sec, buf, sizeof(buf));
  oled.setCursor(40, 44);
  oled.print(buf);

  oled.display();
}

void display_render(Screen screen,
                    const struct tm& local_time,
                    float ambient_temp_c,
                    uint32_t running_sec,
                    uint32_t walking_sec,
                    uint32_t other_sec,
                    bool time_synced) {
  if (screen == SCREEN_WATCHFACE) {
    render_watchface(local_time, ambient_temp_c, time_synced);
  } else {
    render_stats(running_sec, walking_sec, other_sec);
  }
}
