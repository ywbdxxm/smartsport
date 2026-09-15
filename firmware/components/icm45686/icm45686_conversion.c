#include "icm45686_conversion.h"

#define ICM45686_ACCEL_MG_PER_LSB (4000.0f / 32768.0f)
#define ICM45686_GYRO_DPS_PER_LSB (1000.0f / 32768.0f)
#define ICM45686_TEMPERATURE_OFFSET_C 25.0f
#define ICM45686_TEMPERATURE_LSB_PER_C 128.0f

void icm45686_convert_sample(const icm45686_raw_sample_t *raw,
                             icm45686_sample_t *sample) {
  for (int axis = 0; axis < 3; ++axis) {
    sample->accel_mg[axis] = raw->accel[axis] * ICM45686_ACCEL_MG_PER_LSB;
    sample->gyro_dps[axis] = raw->gyro[axis] * ICM45686_GYRO_DPS_PER_LSB;
  }
  sample->temperature_c = ICM45686_TEMPERATURE_OFFSET_C +
                          raw->temperature / ICM45686_TEMPERATURE_LSB_PER_C;
}
