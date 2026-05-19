#pragma once

#include <Arduino.h>
#include "sensors.h"

enum Activity {
  ACT_OTHER   = 0,
  ACT_WALKING = 1,
  ACT_RUNNING = 2,
};

bool activity_begin();

// Push one IMU sample into the rolling window (call at IMU_SAMPLE_HZ).
void activity_push_sample(const ImuSample& s);

// Run classifier on the current window. Returns current best label (with
// hysteresis applied). Should be called at INFER_PERIOD_MS cadence.
Activity activity_infer();

// Most recently committed (hysteresis-filtered) activity.
Activity activity_current();
