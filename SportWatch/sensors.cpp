#include "sensors.h"
#include "config.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

static Adafruit_MPU6050 mpu;
static float last_temp_raw = NAN;

bool sensors_begin() {
  if (!mpu.begin(MPU6050_I2C_ADDR, &Wire)) {
    Serial.println("[sensors] MPU6050 not found");
    return false;
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("[sensors] MPU6050 ready");
  return true;
}

bool sensors_read(ImuSample* out) {
  if (!out) return false;
  sensors_event_t a, g, t;
  if (!mpu.getEvent(&a, &g, &t)) return false;
  out->ax = a.acceleration.x;
  out->ay = a.acceleration.y;
  out->az = a.acceleration.z;
  out->gx = g.gyro.x;
  out->gy = g.gyro.y;
  out->gz = g.gyro.z;
  out->temp_c_raw = t.temperature;
  last_temp_raw = t.temperature;
  return true;
}

float sensors_ambient_temp_c() {
  if (isnan(last_temp_raw)) return NAN;
  return last_temp_raw - MPU_TEMP_OFFSET_C;
}
