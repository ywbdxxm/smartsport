#include "imu_log_throttle.h"

bool imu_log_throttle_due(imu_log_throttle_t *throttle, uint32_t now_ms,
                          uint32_t interval_ms) {
  if (!throttle->has_logged ||
      (uint32_t)(now_ms - throttle->last_log_ms) >= interval_ms) {
    throttle->last_log_ms = now_ms;
    throttle->has_logged = true;
    return true;
  }

  return false;
}
