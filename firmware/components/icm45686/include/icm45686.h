#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "icm45686_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ICM45686_I2C_ADDRESS_AUTO 0x00
#define ICM45686_I2C_ADDRESS_AD0_LOW 0x68
#define ICM45686_I2C_ADDRESS_AD0_HIGH 0x69
#define ICM45686_DEFAULT_I2C_CLOCK_HZ 400000
#define ICM45686_WHO_AM_I_VALUE 0xE9

typedef struct icm45686_t *icm45686_handle_t;

typedef struct {
  i2c_master_bus_handle_t i2c_bus;
  uint32_t i2c_clock_hz;
  uint8_t address;
} icm45686_i2c_config_t;

/**
 * @brief Add and initialize one ICM-45686 on an existing I2C master bus.
 *
 * Address 0 selects automatic probing in the order 0x69, then 0x68. The
 * JYTech module has AD0 pulled high and therefore defaults to 0x69.
 */
esp_err_t icm45686_new_i2c(const icm45686_i2c_config_t *config,
                           icm45686_handle_t *ret_sensor);

/** Read one register-data sample and convert it to mg, degrees/s and degrees C.
 */
esp_err_t icm45686_read_sample(icm45686_handle_t sensor,
                               icm45686_sample_t *sample);

/** Remove the I2C device and release the sensor handle. The caller owns the
 * bus. */
esp_err_t icm45686_del(icm45686_handle_t sensor);

uint8_t icm45686_get_i2c_address(icm45686_handle_t sensor);
const char *icm45686_get_vendor_driver_version(void);

#ifdef __cplusplus
}
#endif
