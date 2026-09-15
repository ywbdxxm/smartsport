#pragma once

#include <stdint.h>

#define INV_IMU_OK 0
#define INV_IMU_ERROR -1
#define INV_IMU_ERROR_TRANSPORT -2
#define INV_IMU_ERROR_TIMEOUT -3
#define INV_IMU_ERROR_BAD_ARG -4

#define UI_I2C 0
#define GYR_STARTUP_TIME_US 70000

#define ACCEL_CONFIG0_ACCEL_UI_FS_SEL_4_G 1
#define GYRO_CONFIG0_GYRO_UI_FS_SEL_1000_DPS 2
#define ACCEL_CONFIG0_ACCEL_ODR_200_HZ 3
#define GYRO_CONFIG0_GYRO_ODR_200_HZ 4
#define IPREG_SYS2_REG_131_ACCEL_UI_LPFBW_DIV_4 5
#define IPREG_SYS1_REG_172_GYRO_UI_LPFBW_DIV_4 6
#define PWR_MGMT0_ACCEL_MODE_LN 7
#define PWR_MGMT0_GYRO_MODE_LN 8

typedef int accel_config0_accel_ui_fs_sel_t;
typedef int gyro_config0_gyro_ui_fs_sel_t;
typedef int accel_config0_accel_odr_t;
typedef int gyro_config0_gyro_odr_t;
typedef int ipreg_sys2_reg_131_accel_ui_lpfbw_t;
typedef int ipreg_sys1_reg_172_gyro_ui_lpfbw_sel_t;
typedef int pwr_mgmt0_accel_mode_t;
typedef int pwr_mgmt0_gyro_mode_t;

typedef struct {
  void *context;
  int (*read_reg)(void *context, uint8_t reg, uint8_t *buffer, uint32_t length);
  int (*write_reg)(void *context, uint8_t reg, const uint8_t *buffer,
                   uint32_t length);
  uint32_t serif_type;
  void (*sleep_us)(uint32_t delay_us);
} inv_imu_transport_t;

typedef struct {
  inv_imu_transport_t transport;
} inv_imu_device_t;

typedef struct {
  int16_t accel_data[3];
  int16_t gyro_data[3];
  int16_t temp_data;
} inv_imu_sensor_data_t;

int inv_imu_get_who_am_i(inv_imu_device_t *sensor, uint8_t *who_am_i);
int inv_imu_soft_reset(inv_imu_device_t *sensor);
int inv_imu_set_accel_fsr(inv_imu_device_t *sensor,
                          accel_config0_accel_ui_fs_sel_t range);
int inv_imu_set_gyro_fsr(inv_imu_device_t *sensor,
                         gyro_config0_gyro_ui_fs_sel_t range);
int inv_imu_set_accel_frequency(inv_imu_device_t *sensor,
                                accel_config0_accel_odr_t frequency);
int inv_imu_set_gyro_frequency(inv_imu_device_t *sensor,
                               gyro_config0_gyro_odr_t frequency);
int inv_imu_set_accel_ln_bw(inv_imu_device_t *sensor,
                            ipreg_sys2_reg_131_accel_ui_lpfbw_t bandwidth);
int inv_imu_set_gyro_ln_bw(inv_imu_device_t *sensor,
                           ipreg_sys1_reg_172_gyro_ui_lpfbw_sel_t bandwidth);
int inv_imu_set_accel_mode(inv_imu_device_t *sensor,
                           pwr_mgmt0_accel_mode_t mode);
int inv_imu_set_gyro_mode(inv_imu_device_t *sensor, pwr_mgmt0_gyro_mode_t mode);
int inv_imu_get_register_data(inv_imu_device_t *sensor,
                              inv_imu_sensor_data_t *data);
const char *inv_imu_get_version(void);
