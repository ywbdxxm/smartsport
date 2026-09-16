#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t last_log_ms;
  bool has_logged;
} imu_log_throttle_t;

bool imu_log_throttle_due(imu_log_throttle_t *throttle, uint32_t now_ms,
                          uint32_t interval_ms);
