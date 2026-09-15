#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float accel_mg[3];
  float gyro_dps[3];
  float temperature_c;
} icm45686_sample_t;

#ifdef __cplusplus
}
#endif
