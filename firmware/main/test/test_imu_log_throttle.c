#include "imu_log_throttle.h"

#include <stdio.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #condition);                                                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int test_limits_success_logs_to_one_per_second(void) {
  imu_log_throttle_t throttle = {0};

  CHECK(imu_log_throttle_due(&throttle, 0, 1000));
  CHECK(!imu_log_throttle_due(&throttle, 100, 1000));
  CHECK(!imu_log_throttle_due(&throttle, 999, 1000));
  CHECK(imu_log_throttle_due(&throttle, 1000, 1000));
  CHECK(!imu_log_throttle_due(&throttle, 1999, 1000));
  CHECK(imu_log_throttle_due(&throttle, 2000, 1000));
  return 0;
}

int main(void) {
  CHECK(test_limits_success_logs_to_one_per_second() == 0);
  puts("IMU log throttle host test passed");
  return 0;
}
