#include "icm45686_conversion.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK_NEAR(actual, expected, tolerance)                                \
  do {                                                                         \
    const float actual_value = (actual);                                       \
    const float expected_value = (expected);                                   \
    if (fabsf(actual_value - expected_value) > (tolerance)) {                  \
      fprintf(stderr,                                                          \
              "CHECK_NEAR failed at %s:%d: actual=%f expected=%f "             \
              "tolerance=%f\n",                                                \
              __FILE__, __LINE__, actual_value, expected_value, (tolerance));  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  const icm45686_raw_sample_t positive = {
      .accel = {32767, 16384, 0},
      .gyro = {32767, 16384, 0},
      .temperature = 128,
  };
  icm45686_sample_t converted = {0};

  icm45686_convert_sample(&positive, &converted);
  CHECK_NEAR(converted.accel_mg[0], 3999.878f, 0.001f);
  CHECK_NEAR(converted.accel_mg[1], 2000.0f, 0.001f);
  CHECK_NEAR(converted.accel_mg[2], 0.0f, 0.001f);
  CHECK_NEAR(converted.gyro_dps[0], 999.969f, 0.001f);
  CHECK_NEAR(converted.gyro_dps[1], 500.0f, 0.001f);
  CHECK_NEAR(converted.temperature_c, 26.0f, 0.001f);

  const icm45686_raw_sample_t negative = {
      .accel = {-32768, -16384, 0},
      .gyro = {-32768, -16384, 0},
      .temperature = -128,
  };
  icm45686_convert_sample(&negative, &converted);
  CHECK_NEAR(converted.accel_mg[0], -4000.0f, 0.001f);
  CHECK_NEAR(converted.accel_mg[1], -2000.0f, 0.001f);
  CHECK_NEAR(converted.gyro_dps[0], -1000.0f, 0.001f);
  CHECK_NEAR(converted.gyro_dps[1], -500.0f, 0.001f);
  CHECK_NEAR(converted.temperature_c, 24.0f, 0.001f);

  puts("ICM-45686 conversion tests passed");
  return 0;
}
