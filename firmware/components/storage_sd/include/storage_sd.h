#pragma once

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_err.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_SD_MOUNT_POINT "/sdcard"

typedef struct storage_sd_context *storage_sd_handle_t;

typedef struct {
  spi_host_device_t host_id;
  gpio_num_t mosi_gpio;
  gpio_num_t miso_gpio;
  gpio_num_t sclk_gpio;
  gpio_num_t cs_gpio;
  gpio_num_t cd_gpio;
  uint32_t max_freq_khz;
} storage_sd_config_t;

/* The returned handle and its SPI host have single-threaded caller ownership.
 */
esp_err_t storage_sd_mount(const storage_sd_config_t *config,
                           storage_sd_handle_t *out_handle);

/* Writes, syncs, verifies, and removes only the component's 32 KiB test file.
 */
esp_err_t storage_sd_run_smoke_test(storage_sd_handle_t handle);

/* Consumes the handle and attempts every cleanup step even if one fails. */
esp_err_t storage_sd_unmount(storage_sd_handle_t handle);

#ifdef __cplusplus
}
#endif
