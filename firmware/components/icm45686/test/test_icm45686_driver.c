#include "freertos/task.h"
#include "icm45686.h"
#include "inv_imu_driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHO_AM_I_REGISTER 0x72
#define ADDRESS_COUNT 128

typedef struct fake_i2c_bus_t {
  bool present[ADDRESS_COUNT];
  bool nack_first_probe[ADDRESS_COUNT];
  uint8_t who_am_i[ADDRESS_COUNT];
  esp_err_t probe_error[ADDRESS_COUNT];
  esp_err_t read_error[ADDRESS_COUNT];
  int probe_count[ADDRESS_COUNT];
  int add_count;
  int remove_count;
  int active_devices;
  int remove_failures_remaining;
} fake_i2c_bus_t;

typedef struct fake_i2c_device_t {
  fake_i2c_bus_t *bus;
  uint8_t address;
} fake_i2c_device_t;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #condition);                                                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static void fake_bus_init(fake_i2c_bus_t *bus) {
  memset(bus, 0, sizeof(*bus));
  for (int address = 0; address < ADDRESS_COUNT; ++address) {
    bus->who_am_i[address] = ICM45686_WHO_AM_I_VALUE;
  }
}

static icm45686_i2c_config_t fake_config(fake_i2c_bus_t *bus) {
  const icm45686_i2c_config_t config = {
      .i2c_bus = bus,
      .i2c_clock_hz = ICM45686_DEFAULT_I2C_CLOCK_HZ,
      .address = ICM45686_I2C_ADDRESS_AUTO,
  };
  return config;
}

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address,
                           int xfer_timeout_ms) {
  (void)xfer_timeout_ms;
  fake_i2c_bus_t *bus = bus_handle;
  ++bus->probe_count[address];
  if (bus->probe_error[address] != ESP_OK) {
    return bus->probe_error[address];
  }
  if (bus->nack_first_probe[address] && bus->probe_count[address] == 1) {
    return ESP_ERR_NOT_FOUND;
  }
  return bus->present[address] ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle,
                                    const i2c_device_config_t *device_config,
                                    i2c_master_dev_handle_t *ret_handle) {
  fake_i2c_device_t *device = malloc(sizeof(*device));
  if (device == NULL) {
    return ESP_ERR_NO_MEM;
  }
  device->bus = bus_handle;
  device->address = (uint8_t)device_config->device_address;
  *ret_handle = device;
  ++device->bus->add_count;
  ++device->bus->active_devices;
  return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t i2c_dev) {
  fake_i2c_device_t *device = i2c_dev;
  ++device->bus->remove_count;
  if (device->bus->remove_failures_remaining > 0) {
    --device->bus->remove_failures_remaining;
    return ESP_FAIL;
  }
  --device->bus->active_devices;
  free(device);
  return ESP_OK;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev,
                                      const uint8_t *write_buffer,
                                      size_t write_size, uint8_t *read_buffer,
                                      size_t read_size, int xfer_timeout_ms) {
  (void)xfer_timeout_ms;
  fake_i2c_device_t *device = i2c_dev;
  if (device->bus->read_error[device->address] != ESP_OK) {
    return device->bus->read_error[device->address];
  }
  if (write_size != 1 || read_size == 0 || write_buffer == NULL ||
      read_buffer == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  memset(read_buffer, 0, read_size);
  if (*write_buffer == WHO_AM_I_REGISTER) {
    read_buffer[0] = device->bus->who_am_i[device->address];
  }
  return ESP_OK;
}

esp_err_t i2c_master_multi_buffer_transmit(
    i2c_master_dev_handle_t i2c_dev,
    i2c_master_transmit_multi_buffer_info_t *buffer_info_array,
    size_t array_size, int xfer_timeout_ms) {
  (void)i2c_dev;
  (void)buffer_info_array;
  (void)array_size;
  (void)xfer_timeout_ms;
  return ESP_OK;
}

void esp_rom_delay_us(uint32_t delay_us) { (void)delay_us; }

void vTaskDelay(TickType_t delay_ticks) { (void)delay_ticks; }

int inv_imu_get_who_am_i(inv_imu_device_t *sensor, uint8_t *who_am_i) {
  int result = sensor->transport.read_reg(sensor->transport.context,
                                          WHO_AM_I_REGISTER, who_am_i, 1);
  if (result != 0 && sensor->transport.serif_type == UI_I2C) {
    result = sensor->transport.read_reg(sensor->transport.context,
                                        WHO_AM_I_REGISTER, who_am_i, 1);
  }
  return result == 0 ? INV_IMU_OK : INV_IMU_ERROR_TRANSPORT;
}

int inv_imu_soft_reset(inv_imu_device_t *sensor) {
  (void)sensor;
  return INV_IMU_OK;
}

int inv_imu_set_accel_fsr(inv_imu_device_t *sensor,
                          accel_config0_accel_ui_fs_sel_t range) {
  (void)sensor;
  (void)range;
  return INV_IMU_OK;
}

int inv_imu_set_gyro_fsr(inv_imu_device_t *sensor,
                         gyro_config0_gyro_ui_fs_sel_t range) {
  (void)sensor;
  (void)range;
  return INV_IMU_OK;
}

int inv_imu_set_accel_frequency(inv_imu_device_t *sensor,
                                accel_config0_accel_odr_t frequency) {
  (void)sensor;
  (void)frequency;
  return INV_IMU_OK;
}

int inv_imu_set_gyro_frequency(inv_imu_device_t *sensor,
                               gyro_config0_gyro_odr_t frequency) {
  (void)sensor;
  (void)frequency;
  return INV_IMU_OK;
}

int inv_imu_set_accel_ln_bw(inv_imu_device_t *sensor,
                            ipreg_sys2_reg_131_accel_ui_lpfbw_t bandwidth) {
  (void)sensor;
  (void)bandwidth;
  return INV_IMU_OK;
}

int inv_imu_set_gyro_ln_bw(inv_imu_device_t *sensor,
                           ipreg_sys1_reg_172_gyro_ui_lpfbw_sel_t bandwidth) {
  (void)sensor;
  (void)bandwidth;
  return INV_IMU_OK;
}

int inv_imu_set_accel_mode(inv_imu_device_t *sensor,
                           pwr_mgmt0_accel_mode_t mode) {
  (void)sensor;
  (void)mode;
  return INV_IMU_OK;
}

int inv_imu_set_gyro_mode(inv_imu_device_t *sensor,
                          pwr_mgmt0_gyro_mode_t mode) {
  (void)sensor;
  (void)mode;
  return INV_IMU_OK;
}

int inv_imu_get_register_data(inv_imu_device_t *sensor,
                              inv_imu_sensor_data_t *data) {
  (void)sensor;
  memset(data, 0, sizeof(*data));
  return INV_IMU_OK;
}

const char *inv_imu_get_version(void) { return "host-fake"; }

static int test_retries_first_probe_nack(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.present[ICM45686_I2C_ADDRESS_AD0_HIGH] = true;
  bus.nack_first_probe[ICM45686_I2C_ADDRESS_AD0_HIGH] = true;
  icm45686_i2c_config_t config = fake_config(&bus);
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_OK);
  CHECK(icm45686_get_i2c_address(sensor) == ICM45686_I2C_ADDRESS_AD0_HIGH);
  CHECK(bus.probe_count[ICM45686_I2C_ADDRESS_AD0_HIGH] == 2);
  CHECK(icm45686_del(sensor) == ESP_OK);
  CHECK(bus.active_devices == 0);
  return 0;
}

static int test_uses_low_address_when_high_is_absent(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.present[ICM45686_I2C_ADDRESS_AD0_LOW] = true;
  icm45686_i2c_config_t config = fake_config(&bus);
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_OK);
  CHECK(icm45686_get_i2c_address(sensor) == ICM45686_I2C_ADDRESS_AD0_LOW);
  CHECK(bus.probe_count[ICM45686_I2C_ADDRESS_AD0_HIGH] == 2);
  CHECK(bus.probe_count[ICM45686_I2C_ADDRESS_AD0_LOW] == 1);
  CHECK(icm45686_del(sensor) == ESP_OK);
  return 0;
}

static int test_skips_acknowledging_device_with_wrong_identity(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.present[ICM45686_I2C_ADDRESS_AD0_HIGH] = true;
  bus.who_am_i[ICM45686_I2C_ADDRESS_AD0_HIGH] = 0x00;
  bus.present[ICM45686_I2C_ADDRESS_AD0_LOW] = true;
  icm45686_i2c_config_t config = fake_config(&bus);
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_OK);
  CHECK(icm45686_get_i2c_address(sensor) == ICM45686_I2C_ADDRESS_AD0_LOW);
  CHECK(bus.add_count == 2);
  CHECK(bus.remove_count == 1);
  CHECK(bus.active_devices == 1);
  CHECK(icm45686_del(sensor) == ESP_OK);
  return 0;
}

static int test_propagates_probe_timeout(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.probe_error[ICM45686_I2C_ADDRESS_AD0_HIGH] = ESP_ERR_TIMEOUT;
  icm45686_i2c_config_t config = fake_config(&bus);
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_ERR_TIMEOUT);
  CHECK(sensor == NULL);
  CHECK(bus.add_count == 0);
  return 0;
}

static int test_cleans_up_device_after_identity_mismatch(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.present[ICM45686_I2C_ADDRESS_AD0_HIGH] = true;
  bus.who_am_i[ICM45686_I2C_ADDRESS_AD0_HIGH] = 0x00;
  icm45686_i2c_config_t config = fake_config(&bus);
  config.address = ICM45686_I2C_ADDRESS_AD0_HIGH;
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_ERR_INVALID_RESPONSE);
  CHECK(sensor == NULL);
  CHECK(bus.add_count == 1);
  CHECK(bus.remove_count == 1);
  CHECK(bus.active_devices == 0);
  return 0;
}

static int test_propagates_identity_read_timeout_and_cleans_up(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.present[ICM45686_I2C_ADDRESS_AD0_HIGH] = true;
  bus.read_error[ICM45686_I2C_ADDRESS_AD0_HIGH] = ESP_ERR_TIMEOUT;
  icm45686_i2c_config_t config = fake_config(&bus);
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_ERR_TIMEOUT);
  CHECK(sensor == NULL);
  CHECK(bus.add_count == 1);
  CHECK(bus.remove_count == 1);
  CHECK(bus.active_devices == 0);
  return 0;
}

static int test_delete_can_retry_after_remove_failure(void) {
  fake_i2c_bus_t bus;
  fake_bus_init(&bus);
  bus.present[ICM45686_I2C_ADDRESS_AD0_HIGH] = true;
  icm45686_i2c_config_t config = fake_config(&bus);
  icm45686_handle_t sensor = NULL;

  CHECK(icm45686_new_i2c(&config, &sensor) == ESP_OK);
  bus.remove_failures_remaining = 1;
  CHECK(icm45686_del(sensor) == ESP_FAIL);
  CHECK(bus.active_devices == 1);
  CHECK(icm45686_del(sensor) == ESP_OK);
  CHECK(bus.active_devices == 0);
  return 0;
}

int main(void) {
  CHECK(test_retries_first_probe_nack() == 0);
  CHECK(test_uses_low_address_when_high_is_absent() == 0);
  CHECK(test_skips_acknowledging_device_with_wrong_identity() == 0);
  CHECK(test_propagates_probe_timeout() == 0);
  CHECK(test_cleans_up_device_after_identity_mismatch() == 0);
  CHECK(test_propagates_identity_read_timeout_and_cleans_up() == 0);
  CHECK(test_delete_can_retry_after_remove_failure() == 0);
  puts("ICM-45686 driver host tests passed");
  return 0;
}
