#include "icm45686.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm45686_conversion.h"
#include "inv_imu_driver.h"
#include <stdbool.h>
#include <stdlib.h>

#define ICM45686_I2C_TIMEOUT_MS 100
#define ICM45686_PROBE_TIMEOUT_MS 50
#define ICM45686_PROBE_ATTEMPTS 2
#define ICM45686_POWER_UP_DELAY_US 3000
#define ICM45686_TASK_DELAY_THRESHOLD_US 10000

static const char *TAG = "icm45686";

struct icm45686_t {
  i2c_master_dev_handle_t i2c_device;
  inv_imu_device_t vendor_device;
  esp_err_t transport_error;
  uint8_t address;
  bool initialized;
};

static int icm45686_i2c_read(void *context, uint8_t reg, uint8_t *buffer,
                             uint32_t length) {
  icm45686_handle_t sensor = context;
  if (sensor == NULL || buffer == NULL || length == 0) {
    return -1;
  }

  esp_err_t error =
      i2c_master_transmit_receive(sensor->i2c_device, &reg, sizeof(reg), buffer,
                                  length, ICM45686_I2C_TIMEOUT_MS);
  if (error != ESP_OK) {
    sensor->transport_error = error;
  }
  return error == ESP_OK ? 0 : -1;
}

static int icm45686_i2c_write(void *context, uint8_t reg, const uint8_t *buffer,
                              uint32_t length) {
  icm45686_handle_t sensor = context;
  if (sensor == NULL || buffer == NULL || length == 0) {
    return -1;
  }

  i2c_master_transmit_multi_buffer_info_t buffers[] = {
      {.write_buffer = &reg, .buffer_size = sizeof(reg)},
      {.write_buffer = buffer, .buffer_size = length},
  };
  esp_err_t error = i2c_master_multi_buffer_transmit(
      sensor->i2c_device, buffers, sizeof(buffers) / sizeof(buffers[0]),
      ICM45686_I2C_TIMEOUT_MS);
  if (error != ESP_OK) {
    sensor->transport_error = error;
  }
  return error == ESP_OK ? 0 : -1;
}

static void icm45686_sleep_us(uint32_t delay_us) {
  if (delay_us >= ICM45686_TASK_DELAY_THRESHOLD_US) {
    const uint32_t delay_ms = (delay_us + 999) / 1000;
    TickType_t delay_ticks = pdMS_TO_TICKS(delay_ms);
    vTaskDelay(delay_ticks > 0 ? delay_ticks : 1);
  } else {
    esp_rom_delay_us(delay_us);
  }
}

static esp_err_t icm45686_vendor_result(icm45686_handle_t sensor,
                                        int vendor_result) {
  if (vendor_result == INV_IMU_OK) {
    return ESP_OK;
  }
  if (sensor->transport_error != ESP_OK) {
    return sensor->transport_error;
  }
  if (vendor_result == INV_IMU_ERROR_BAD_ARG) {
    return ESP_ERR_INVALID_ARG;
  }
  if (vendor_result == INV_IMU_ERROR_TIMEOUT) {
    return ESP_ERR_TIMEOUT;
  }
  return ESP_FAIL;
}

static esp_err_t icm45686_probe_candidate(i2c_master_bus_handle_t bus,
                                          uint8_t address) {
  esp_err_t error = ESP_ERR_NOT_FOUND;
  for (int attempt = 0; attempt < ICM45686_PROBE_ATTEMPTS; ++attempt) {
    error = i2c_master_probe(bus, address, ICM45686_PROBE_TIMEOUT_MS);
    if (error != ESP_ERR_NOT_FOUND) {
      break;
    }
  }
  return error;
}

static esp_err_t icm45686_configure(icm45686_handle_t sensor) {
  int result;
  uint8_t who_am_i = 0;

  sensor->transport_error = ESP_OK;
  result = inv_imu_get_who_am_i(&sensor->vendor_device, &who_am_i);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to read WHO_AM_I");
  ESP_RETURN_ON_FALSE(who_am_i == ICM45686_WHO_AM_I_VALUE,
                      ESP_ERR_INVALID_RESPONSE, TAG,
                      "unexpected WHO_AM_I 0x%02x (expected 0x%02x)", who_am_i,
                      ICM45686_WHO_AM_I_VALUE);

  sensor->transport_error = ESP_OK;
  result = inv_imu_soft_reset(&sensor->vendor_device);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "soft reset failed");

  sensor->transport_error = ESP_OK;
  result = inv_imu_set_accel_fsr(&sensor->vendor_device,
                                 ACCEL_CONFIG0_ACCEL_UI_FS_SEL_4_G);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to set accelerometer range");

  sensor->transport_error = ESP_OK;
  result = inv_imu_set_gyro_fsr(&sensor->vendor_device,
                                GYRO_CONFIG0_GYRO_UI_FS_SEL_1000_DPS);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to set gyroscope range");

  sensor->transport_error = ESP_OK;
  result = inv_imu_set_accel_frequency(&sensor->vendor_device,
                                       ACCEL_CONFIG0_ACCEL_ODR_200_HZ);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to set accelerometer ODR");

  sensor->transport_error = ESP_OK;
  result = inv_imu_set_gyro_frequency(&sensor->vendor_device,
                                      GYRO_CONFIG0_GYRO_ODR_200_HZ);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to set gyroscope ODR");

  sensor->transport_error = ESP_OK;
  result = inv_imu_set_accel_ln_bw(&sensor->vendor_device,
                                   IPREG_SYS2_REG_131_ACCEL_UI_LPFBW_DIV_4);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to set accelerometer bandwidth");

  sensor->transport_error = ESP_OK;
  result = inv_imu_set_gyro_ln_bw(&sensor->vendor_device,
                                  IPREG_SYS1_REG_172_GYRO_UI_LPFBW_DIV_4);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to set gyroscope bandwidth");

  sensor->transport_error = ESP_OK;
  result =
      inv_imu_set_accel_mode(&sensor->vendor_device, PWR_MGMT0_ACCEL_MODE_LN);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to enable accelerometer");

  sensor->transport_error = ESP_OK;
  result =
      inv_imu_set_gyro_mode(&sensor->vendor_device, PWR_MGMT0_GYRO_MODE_LN);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "failed to enable gyroscope");

  icm45686_sleep_us(GYR_STARTUP_TIME_US);
  sensor->initialized = true;
  return ESP_OK;
}

static esp_err_t icm45686_create_at_address(const icm45686_i2c_config_t *config,
                                            uint8_t address,
                                            icm45686_handle_t *ret_sensor) {
  icm45686_handle_t sensor = calloc(1, sizeof(*sensor));
  ESP_RETURN_ON_FALSE(sensor != NULL, ESP_ERR_NO_MEM, TAG,
                      "failed to allocate sensor handle");
  sensor->transport_error = ESP_OK;
  sensor->address = address;

  const i2c_device_config_t device_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = config->i2c_clock_hz,
  };
  esp_err_t error = i2c_master_bus_add_device(config->i2c_bus, &device_config,
                                              &sensor->i2c_device);
  if (error != ESP_OK) {
    free(sensor);
    return error;
  }

  sensor->vendor_device.transport.context = sensor;
  sensor->vendor_device.transport.read_reg = icm45686_i2c_read;
  sensor->vendor_device.transport.write_reg = icm45686_i2c_write;
  sensor->vendor_device.transport.serif_type = UI_I2C;
  sensor->vendor_device.transport.sleep_us = icm45686_sleep_us;

  error = icm45686_configure(sensor);
  if (error != ESP_OK) {
    i2c_master_bus_rm_device(sensor->i2c_device);
    free(sensor);
    return error;
  }

  *ret_sensor = sensor;
  return ESP_OK;
}

esp_err_t icm45686_new_i2c(const icm45686_i2c_config_t *config,
                           icm45686_handle_t *ret_sensor) {
  ESP_RETURN_ON_FALSE(config != NULL && ret_sensor != NULL, ESP_ERR_INVALID_ARG,
                      TAG, "invalid argument");
  ESP_RETURN_ON_FALSE(config->i2c_bus != NULL, ESP_ERR_INVALID_ARG, TAG,
                      "I2C bus is required");
  ESP_RETURN_ON_FALSE(config->i2c_clock_hz > 0, ESP_ERR_INVALID_ARG, TAG,
                      "I2C clock must be greater than zero");
  ESP_RETURN_ON_FALSE(config->address == ICM45686_I2C_ADDRESS_AUTO ||
                          config->address == ICM45686_I2C_ADDRESS_AD0_LOW ||
                          config->address == ICM45686_I2C_ADDRESS_AD0_HIGH,
                      ESP_ERR_INVALID_ARG, TAG,
                      "I2C address must be 0x68, 0x69, or AUTO");

  *ret_sensor = NULL;
  icm45686_sleep_us(ICM45686_POWER_UP_DELAY_US);

  const bool automatic = config->address == ICM45686_I2C_ADDRESS_AUTO;
  const uint8_t candidates[] = {ICM45686_I2C_ADDRESS_AD0_HIGH,
                                ICM45686_I2C_ADDRESS_AD0_LOW};
  const size_t candidate_count =
      automatic ? sizeof(candidates) / sizeof(candidates[0]) : 1;
  esp_err_t detection_error = ESP_ERR_NOT_FOUND;

  for (size_t index = 0; index < candidate_count; ++index) {
    const uint8_t address = automatic ? candidates[index] : config->address;
    esp_err_t error = icm45686_probe_candidate(config->i2c_bus, address);
    if (error == ESP_ERR_NOT_FOUND) {
      continue;
    }
    if (error != ESP_OK) {
      return error;
    }

    error = icm45686_create_at_address(config, address, ret_sensor);
    if (error == ESP_ERR_INVALID_RESPONSE && automatic) {
      detection_error = error;
      continue;
    }
    if (error != ESP_OK) {
      return error;
    }

    ESP_LOGI(TAG, "ICM-45686 detected at 0x%02x, vendor driver %s", address,
             inv_imu_get_version());
    return ESP_OK;
  }

  return detection_error;
}

esp_err_t icm45686_read_sample(icm45686_handle_t sensor,
                               icm45686_sample_t *sample) {
  ESP_RETURN_ON_FALSE(sensor != NULL && sample != NULL, ESP_ERR_INVALID_ARG,
                      TAG, "invalid argument");
  ESP_RETURN_ON_FALSE(sensor->initialized, ESP_ERR_INVALID_STATE, TAG,
                      "sensor is not initialized");

  inv_imu_sensor_data_t vendor_sample;
  sensor->transport_error = ESP_OK;
  int result =
      inv_imu_get_register_data(&sensor->vendor_device, &vendor_sample);
  ESP_RETURN_ON_ERROR(icm45686_vendor_result(sensor, result), TAG,
                      "sample read failed");

  const icm45686_raw_sample_t raw = {
      .accel = {vendor_sample.accel_data[0], vendor_sample.accel_data[1],
                vendor_sample.accel_data[2]},
      .gyro = {vendor_sample.gyro_data[0], vendor_sample.gyro_data[1],
               vendor_sample.gyro_data[2]},
      .temperature = vendor_sample.temp_data,
  };
  icm45686_convert_sample(&raw, sample);
  return ESP_OK;
}

esp_err_t icm45686_del(icm45686_handle_t sensor) {
  ESP_RETURN_ON_FALSE(sensor != NULL, ESP_ERR_INVALID_ARG, TAG,
                      "sensor handle is required");
  esp_err_t error = i2c_master_bus_rm_device(sensor->i2c_device);
  if (error != ESP_OK) {
    return error;
  }
  free(sensor);
  return ESP_OK;
}

uint8_t icm45686_get_i2c_address(icm45686_handle_t sensor) {
  return sensor == NULL ? 0 : sensor->address;
}

const char *icm45686_get_vendor_driver_version(void) {
  return inv_imu_get_version();
}
