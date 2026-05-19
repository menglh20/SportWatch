#include "activity.h"
#include "config.h"
#include <math.h>

// Placeholder classifier. Phase B will swap this for an Edge Impulse model:
//   #include <SportWatch_inferencing.h>
//   ei_impulse_result_t result;
//   run_classifier(&signal, &result, false);
// Until the model is trained and exported, we run a coarse acceleration-
// magnitude threshold so the rest of the pipeline (stats, display, NVS) is
// testable end-to-end.

static constexpr size_t WINDOW_N = IMU_SAMPLE_HZ * 2;  // 2-second window
static float mag_buf[WINDOW_N];
static size_t mag_head = 0;
static size_t mag_count = 0;

static Activity last_raw = ACT_OTHER;
static Activity committed = ACT_OTHER;
static uint8_t streak = 0;
static constexpr uint8_t HYSTERESIS_N = 3;

bool activity_begin() {
  mag_head = 0;
  mag_count = 0;
  return true;
}

void activity_push_sample(const ImuSample& s) {
  float m = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  mag_buf[mag_head] = m;
  mag_head = (mag_head + 1) % WINDOW_N;
  if (mag_count < WINDOW_N) mag_count++;
}

static Activity classify_threshold() {
  if (mag_count < WINDOW_N) return ACT_OTHER;
  float mean = 0;
  for (size_t i = 0; i < WINDOW_N; ++i) mean += mag_buf[i];
  mean /= WINDOW_N;
  float var = 0;
  for (size_t i = 0; i < WINDOW_N; ++i) {
    float d = mag_buf[i] - mean;
    var += d * d;
  }
  var /= WINDOW_N;
  float std_dev = sqrtf(var);
  // Empirical thresholds; tune on bench, replace with TinyML model.
  if (std_dev > 6.0f) return ACT_RUNNING;
  if (std_dev > 1.5f) return ACT_WALKING;
  return ACT_OTHER;
}

Activity activity_infer() {
  Activity raw = classify_threshold();
  if (raw == last_raw) {
    if (streak < 255) streak++;
  } else {
    last_raw = raw;
    streak = 1;
  }
  if (streak >= HYSTERESIS_N) committed = raw;
  return committed;
}

Activity activity_current() { return committed; }
