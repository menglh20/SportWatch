#pragma once

#define I2C_SDA_PIN          8
#define I2C_SCL_PIN          9
#define I2C_FREQ_HZ          400000

#define MPU6050_I2C_ADDR     0x68
#define SSD1306_I2C_ADDR     0x3C
#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        64
#define OLED_RESET           -1

#define IMU_SAMPLE_HZ        50
#define IMU_SAMPLE_PERIOD_MS (1000 / IMU_SAMPLE_HZ)

#define INFER_PERIOD_MS      500
#define DISPLAY_PERIOD_MS    1000
#define SCREEN_SWITCH_MS     5000
#define STATS_SAVE_MS        60000

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define NTP_SERVER_1         "pool.ntp.org"
#define NTP_SERVER_2         "time.google.com"
#define NTP_SERVER_3         "time.windows.com"

#define TIMEZONE_POSIX       "PST8PDT,M3.2.0,M11.1.0"

#define DAILY_RESYNC_HOUR    3

#define MPU_TEMP_OFFSET_C    8.0f

#define AP_CONFIG_SSID       "SportWatch-Setup"
#define AP_CONFIG_PASSWORD   "12345678"
