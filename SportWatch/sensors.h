#pragma once

#include <Arduino.h>

struct ImuSample {
  float ax, ay, az;
  float gx, gy, gz;
  float temp_c_raw;
};

bool sensors_begin();
bool sensors_read(ImuSample* out);
float sensors_ambient_temp_c();
