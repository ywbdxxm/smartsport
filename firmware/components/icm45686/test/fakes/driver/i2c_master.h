#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef struct fake_i2c_bus_t *i2c_master_bus_handle_t;
typedef struct fake_i2c_device_t *i2c_master_dev_handle_t;

#define I2C_ADDR_BIT_LEN_7 0

typedef struct {
  int dev_addr_length;
  uint16_t device_address;
  uint32_t scl_speed_hz;
} i2c_device_config_t;

typedef struct {
  const uint8_t *write_buffer;
  size_t buffer_size;
} i2c_master_transmit_multi_buffer_info_t;

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address,
                           int xfer_timeout_ms);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle,
                                    const i2c_device_config_t *device_config,
                                    i2c_master_dev_handle_t *ret_handle);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t i2c_dev);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev,
                                      const uint8_t *write_buffer,
                                      size_t write_size, uint8_t *read_buffer,
                                      size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_master_multi_buffer_transmit(
    i2c_master_dev_handle_t i2c_dev,
    i2c_master_transmit_multi_buffer_info_t *buffer_info_array,
    size_t array_size, int xfer_timeout_ms);
