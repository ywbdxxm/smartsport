#pragma once

#include "icm45686_types.h"
#include <stdint.h>

typedef struct {
  int16_t accel[3];
  int16_t gyro[3];
  int16_t temperature;
} icm45686_raw_sample_t;

void icm45686_convert_sample(const icm45686_raw_sample_t *raw,
                             icm45686_sample_t *sample);
