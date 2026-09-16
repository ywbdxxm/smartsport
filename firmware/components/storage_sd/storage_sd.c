#include "storage_sd.h"

#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "storage_sd_pattern.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define STORAGE_SD_MIN_FREQ_KHZ 400U
#define STORAGE_SD_MAX_FREQ_KHZ 20000U
#define STORAGE_SD_MAX_OPEN_FILES 4
#define STORAGE_SD_ALLOCATION_UNIT_SIZE (16U * 1024U)
#define STORAGE_SD_TRANSFER_SIZE 4096
#define STORAGE_SD_SMOKE_TEST_SIZE (32U * 1024U)
#define STORAGE_SD_SMOKE_CHUNK_SIZE 512U
#define STORAGE_SD_SMOKE_TEST_PATH                                             \
  STORAGE_SD_MOUNT_POINT "/smartsport_sd_test.bin"

struct storage_sd_context {
  spi_host_device_t host_id;
  sdmmc_card_t *card;
  bool mounted;
};

static const char *TAG = "storage_sd";

static bool config_is_valid(const storage_sd_config_t *config) {
  if (config == NULL || !GPIO_IS_VALID_OUTPUT_GPIO(config->mosi_gpio) ||
      !GPIO_IS_VALID_GPIO(config->miso_gpio) ||
      !GPIO_IS_VALID_OUTPUT_GPIO(config->sclk_gpio) ||
      !GPIO_IS_VALID_OUTPUT_GPIO(config->cs_gpio)) {
    return false;
  }

  if (config->cd_gpio != GPIO_NUM_NC && !GPIO_IS_VALID_GPIO(config->cd_gpio)) {
    return false;
  }

  return config->max_freq_khz >= STORAGE_SD_MIN_FREQ_KHZ &&
         config->max_freq_khz <= STORAGE_SD_MAX_FREQ_KHZ;
}

static esp_err_t configure_card_detect(gpio_num_t cd_gpio) {
  if (cd_gpio == GPIO_NUM_NC) {
    ESP_LOGI(TAG, "Card-detect GPIO is not configured");
    return ESP_OK;
  }

  const gpio_config_t cd_config = {
      .pin_bit_mask = 1ULL << cd_gpio,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t error = gpio_config(&cd_config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure card-detect GPIO%d: %s", cd_gpio,
             esp_err_to_name(error));
    return error;
  }

  ESP_LOGI(TAG,
           "Card-detect GPIO%d raw level=%d (inserted polarity not yet "
           "calibrated)",
           cd_gpio, gpio_get_level(cd_gpio));
  return ESP_OK;
}

static void log_stdio_error(const char *operation, const char *path,
                            int error_number) {
  ESP_LOGE(TAG, "%s failed for %s: errno=%d", operation, path, error_number);
}

static esp_err_t close_stream(FILE **stream, const char *path) {
  if (*stream == NULL) {
    return ESP_OK;
  }

  FILE *closing = *stream;
  *stream = NULL;
  if (fclose(closing) != 0) {
    log_stdio_error("fclose", path, errno);
    return ESP_FAIL;
  }
  return ESP_OK;
}

static void keep_first_error(esp_err_t *result, esp_err_t candidate) {
  if (*result == ESP_OK && candidate != ESP_OK) {
    *result = candidate;
  }
}

esp_err_t storage_sd_mount(const storage_sd_config_t *config,
                           storage_sd_handle_t *out_handle) {
  if (out_handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  *out_handle = NULL;
  if (!config_is_valid(config)) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t error = configure_card_detect(config->cd_gpio);
  if (error != ESP_OK) {
    return error;
  }

  const spi_bus_config_t bus_config = {
      .mosi_io_num = config->mosi_gpio,
      .miso_io_num = config->miso_gpio,
      .sclk_io_num = config->sclk_gpio,
      .quadwp_io_num = GPIO_NUM_NC,
      .quadhd_io_num = GPIO_NUM_NC,
      .max_transfer_sz = STORAGE_SD_TRANSFER_SIZE,
  };
  error = spi_bus_initialize(config->host_id, &bus_config, SDSPI_DEFAULT_DMA);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPI host %d: %s", config->host_id,
             esp_err_to_name(error));
    return error;
  }

  storage_sd_handle_t handle = calloc(1, sizeof(*handle));
  if (handle == NULL) {
    ESP_LOGE(TAG, "Failed to allocate SD context");
    esp_err_t cleanup_error = spi_bus_free(config->host_id);
    if (cleanup_error != ESP_OK) {
      ESP_LOGE(TAG, "Failed to release SPI host after allocation error: %s",
               esp_err_to_name(cleanup_error));
    }
    return ESP_ERR_NO_MEM;
  }
  handle->host_id = config->host_id;

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = config->host_id;
  host.max_freq_khz = (int)config->max_freq_khz;

  sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  device_config.host_id = config->host_id;
  device_config.gpio_cs = config->cs_gpio;
  device_config.gpio_cd = SDSPI_SLOT_NO_CD;

  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = STORAGE_SD_MAX_OPEN_FILES,
      .allocation_unit_size = STORAGE_SD_ALLOCATION_UNIT_SIZE,
  };

  ESP_LOGI(TAG,
           "Mounting SD at %s via SPI host %d, CS GPIO%d, requested clock "
           "%" PRIu32 " kHz",
           STORAGE_SD_MOUNT_POINT, config->host_id, config->cs_gpio,
           config->max_freq_khz);
  error = esp_vfs_fat_sdspi_mount(STORAGE_SD_MOUNT_POINT, &host, &device_config,
                                  &mount_config, &handle->card);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "SD mount failed without formatting: %s",
             esp_err_to_name(error));
    esp_err_t cleanup_error = spi_bus_free(config->host_id);
    if (cleanup_error != ESP_OK) {
      ESP_LOGE(TAG, "Failed to release SPI host after mount error: %s",
               esp_err_to_name(cleanup_error));
    }
    free(handle);
    return error;
  }

  handle->mounted = true;
  sdmmc_card_print_info(stdout, handle->card);
  ESP_LOGI(TAG,
           "SD ready: name=%s, capacity=%" PRIu64
           " MiB, negotiated clock=%d kHz",
           handle->card->cid.name,
           (uint64_t)handle->card->csd.capacity *
               handle->card->csd.sector_size / (1024U * 1024U),
           handle->card->real_freq_khz);
  *out_handle = handle;
  return ESP_OK;
}

esp_err_t storage_sd_run_smoke_test(storage_sd_handle_t handle) {
  if (handle == NULL || !handle->mounted || handle->card == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t result = ESP_OK;
  FILE *stream = NULL;
  bool test_file_created = false;
  uint8_t buffer[STORAGE_SD_SMOKE_CHUNK_SIZE];

  ESP_LOGI(TAG, "Starting 32 KiB write/sync/readback test: %s",
           STORAGE_SD_SMOKE_TEST_PATH);
  stream = fopen(STORAGE_SD_SMOKE_TEST_PATH, "wb");
  if (stream == NULL) {
    log_stdio_error("fopen(wb)", STORAGE_SD_SMOKE_TEST_PATH, errno);
    return ESP_FAIL;
  }
  test_file_created = true;

  for (size_t offset = 0; offset < STORAGE_SD_SMOKE_TEST_SIZE;
       offset += sizeof(buffer)) {
    storage_sd_pattern_fill(buffer, sizeof(buffer), offset);
    if (fwrite(buffer, 1, sizeof(buffer), stream) != sizeof(buffer)) {
      log_stdio_error("fwrite", STORAGE_SD_SMOKE_TEST_PATH, errno);
      result = ESP_FAIL;
      goto cleanup;
    }
  }

  if (fflush(stream) != 0) {
    log_stdio_error("fflush", STORAGE_SD_SMOKE_TEST_PATH, errno);
    result = ESP_FAIL;
    goto cleanup;
  }

  int file_descriptor = fileno(stream);
  if (file_descriptor < 0 || fsync(file_descriptor) != 0) {
    log_stdio_error("fsync", STORAGE_SD_SMOKE_TEST_PATH, errno);
    result = ESP_FAIL;
    goto cleanup;
  }

  result = close_stream(&stream, STORAGE_SD_SMOKE_TEST_PATH);
  if (result != ESP_OK) {
    goto cleanup;
  }

  stream = fopen(STORAGE_SD_SMOKE_TEST_PATH, "rb");
  if (stream == NULL) {
    log_stdio_error("fopen(rb)", STORAGE_SD_SMOKE_TEST_PATH, errno);
    result = ESP_FAIL;
    goto cleanup;
  }

  for (size_t offset = 0; offset < STORAGE_SD_SMOKE_TEST_SIZE;
       offset += sizeof(buffer)) {
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), stream);
    if (bytes_read != sizeof(buffer)) {
      if (ferror(stream)) {
        log_stdio_error("fread", STORAGE_SD_SMOKE_TEST_PATH, errno);
      } else {
        ESP_LOGE(TAG, "Unexpected end of test file at byte %u",
                 (unsigned)(offset + bytes_read));
      }
      result = ESP_FAIL;
      goto cleanup;
    }

    size_t first_bad_offset = 0;
    if (!storage_sd_pattern_matches(buffer, sizeof(buffer), offset,
                                    &first_bad_offset)) {
      ESP_LOGE(TAG, "Readback mismatch at absolute byte %u",
               (unsigned)first_bad_offset);
      result = ESP_ERR_INVALID_CRC;
      goto cleanup;
    }
  }

  int trailing_byte = fgetc(stream);
  if (trailing_byte != EOF || ferror(stream)) {
    ESP_LOGE(TAG, "Test file is not exactly %u bytes",
             (unsigned)STORAGE_SD_SMOKE_TEST_SIZE);
    result = ESP_FAIL;
    goto cleanup;
  }

cleanup:
  keep_first_error(&result, close_stream(&stream, STORAGE_SD_SMOKE_TEST_PATH));
  if (test_file_created && unlink(STORAGE_SD_SMOKE_TEST_PATH) != 0) {
    log_stdio_error("unlink", STORAGE_SD_SMOKE_TEST_PATH, errno);
    keep_first_error(&result, ESP_FAIL);
  }

  if (result == ESP_OK) {
    ESP_LOGI(TAG, "SD 32 KiB write/sync/readback test passed");
  } else {
    ESP_LOGE(TAG, "SD smoke test failed: %s", esp_err_to_name(result));
  }
  return result;
}

esp_err_t storage_sd_unmount(storage_sd_handle_t handle) {
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t result = ESP_OK;
  if (handle->mounted && handle->card != NULL) {
    esp_err_t error =
        esp_vfs_fat_sdcard_unmount(STORAGE_SD_MOUNT_POINT, handle->card);
    if (error != ESP_OK) {
      ESP_LOGE(TAG, "Failed to unmount SD filesystem: %s",
               esp_err_to_name(error));
      keep_first_error(&result, error);
    } else {
      ESP_LOGI(TAG, "SD filesystem unmounted");
    }
    handle->mounted = false;
    handle->card = NULL;
  }

  esp_err_t bus_error = spi_bus_free(handle->host_id);
  if (bus_error != ESP_OK) {
    ESP_LOGE(TAG, "Failed to release SPI host %d: %s", handle->host_id,
             esp_err_to_name(bus_error));
    keep_first_error(&result, bus_error);
  }

  free(handle);
  return result;
}
