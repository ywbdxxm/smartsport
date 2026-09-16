/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "board_config.h"
#include "driver/i2c_master.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm45686.h"
#include "imu_log_throttle.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "storage_sd.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define STATUS_LED_COUNT 1
#define STATUS_LED_RMT_RESOLUTION_HZ (10 * 1000 * 1000)
#define STATUS_LED_BRIGHTNESS 16
#define IMU_SAMPLE_PERIOD_MS 100
#define IMU_LOG_PERIOD_MS 1000
#define IMU_INIT_RETRY_PERIOD_MS 2000
#define LED_TOGGLE_SAMPLE_COUNT 5

static const char *TAG = "smart_sport";
static led_strip_handle_t status_led;

static void configure_status_led(void) {
  const led_strip_config_t strip_config = {
      .strip_gpio_num = BOARD_STATUS_LED_GPIO,
      .max_leds = STATUS_LED_COUNT,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
  };
  const led_strip_rmt_config_t rmt_config = {
      .resolution_hz = STATUS_LED_RMT_RESOLUTION_HZ,
      .flags.with_dma = false,
  };

  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &status_led));
  ESP_ERROR_CHECK(led_strip_clear(status_led));
}

static void set_status_led(uint8_t red, uint8_t green, uint8_t blue) {
  ESP_ERROR_CHECK(led_strip_set_pixel(status_led, 0, red, green, blue));
  ESP_ERROR_CHECK(led_strip_refresh(status_led));
}

static void run_sd_smoke_test(void) {
  const storage_sd_config_t sd_config = {
      .host_id = BOARD_SD_SPI_HOST,
      .mosi_gpio = BOARD_SD_MOSI_GPIO,
      .miso_gpio = BOARD_SD_MISO_GPIO,
      .sclk_gpio = BOARD_SD_SCLK_GPIO,
      .cs_gpio = BOARD_SD_CS_GPIO,
      .cd_gpio = BOARD_SD_CD_GPIO,
      .max_freq_khz = BOARD_SD_MAX_FREQ_KHZ,
  };
  storage_sd_handle_t sd = NULL;

  esp_err_t mount_error = storage_sd_mount(&sd_config, &sd);
  if (mount_error != ESP_OK) {
    ESP_LOGE(TAG,
             "SD unavailable; smoke test skipped and sensor startup will "
             "continue: %s",
             esp_err_to_name(mount_error));
    return;
  }

  esp_err_t smoke_error = storage_sd_run_smoke_test(sd);
  if (smoke_error != ESP_OK) {
    ESP_LOGE(TAG, "SD smoke test failed; sensor startup will continue: %s",
             esp_err_to_name(smoke_error));
  }

  esp_err_t unmount_error = storage_sd_unmount(sd);
  if (unmount_error != ESP_OK) {
    ESP_LOGE(TAG, "SD cleanup failed; sensor startup will continue: %s",
             esp_err_to_name(unmount_error));
  }

  if (smoke_error == ESP_OK && unmount_error == ESP_OK) {
    ESP_LOGI(TAG, "SD startup smoke test completed successfully");
  }
}

static i2c_master_bus_handle_t configure_i2c_bus(void) {
  const i2c_master_bus_config_t bus_config = {
      .i2c_port = BOARD_IMU_I2C_PORT,
      .sda_io_num = BOARD_IMU_I2C_SDA_GPIO,
      .scl_io_num = BOARD_IMU_I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = false,
  };
  i2c_master_bus_handle_t bus = NULL;
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));
  return bus;
}

static icm45686_handle_t wait_for_imu(i2c_master_bus_handle_t i2c_bus) {
  const icm45686_i2c_config_t imu_config = {
      .i2c_bus = i2c_bus,
      .i2c_clock_hz = BOARD_IMU_I2C_CLOCK_HZ,
      .address = ICM45686_I2C_ADDRESS_AUTO,
  };
  icm45686_handle_t imu = NULL;

  while (imu == NULL) {
    esp_err_t error = icm45686_new_i2c(&imu_config, &imu);
    if (error == ESP_OK) {
      break;
    }
    ESP_LOGE(TAG,
             "ICM-45686 init failed: %s. Check VIN 5V, GND, SDA GPIO%d and "
             "SCL GPIO%d.",
             esp_err_to_name(error), BOARD_IMU_I2C_SDA_GPIO,
             BOARD_IMU_I2C_SCL_GPIO);
    set_status_led(STATUS_LED_BRIGHTNESS, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(IMU_INIT_RETRY_PERIOD_MS));
  }
  return imu;
}

void app_main(void) {
  printf("Hello world!\n");

  /* Print chip information */
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU core(s), %s%s%s%s, ", CONFIG_IDF_TARGET,
         chip_info.cores,
         (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
         (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
         (chip_info.features & CHIP_FEATURE_IEEE802154)
             ? ", 802.15.4 (Zigbee/Thread)"
             : "");

  unsigned major_rev = chip_info.revision / 100;
  unsigned minor_rev = chip_info.revision % 100;
  printf("silicon revision v%d.%d, ", major_rev, minor_rev);
  if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {

    printf("Get flash size failed");
    return;
  }

  printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                       : "external");

  printf("Minimum free heap size: %" PRIu32 " bytes\n",
         esp_get_minimum_free_heap_size());

  configure_status_led();
  printf("Onboard WS2812B is on GPIO %d.\n", BOARD_STATUS_LED_GPIO);

  run_sd_smoke_test();

  i2c_master_bus_handle_t i2c_bus = configure_i2c_bus();
  ESP_LOGI(TAG, "ICM-45686 I2C: SDA GPIO%d, SCL GPIO%d, %d Hz",
           BOARD_IMU_I2C_SDA_GPIO, BOARD_IMU_I2C_SCL_GPIO,
           BOARD_IMU_I2C_CLOCK_HZ);
  icm45686_handle_t imu = wait_for_imu(i2c_bus);
  ESP_LOGI(TAG, "ICM-45686 ready at address 0x%02x",
           icm45686_get_i2c_address(imu));

  uint32_t sample_count = 0;
  bool led_on = false;
  imu_log_throttle_t log_throttle = {0};
  while (1) {
    icm45686_sample_t sample;
    esp_err_t error = icm45686_read_sample(imu, &sample);
    if (error == ESP_OK) {
      ++sample_count;
      if (imu_log_throttle_due(&log_throttle, esp_log_timestamp(),
                               IMU_LOG_PERIOD_MS)) {
        printf("IMU #%-6" PRIu32
               " acc(mg): %8.2f %8.2f %8.2f  gyro(dps): %8.2f %8.2f "
               "%8.2f  temp(C): %6.2f\n",
               sample_count, sample.accel_mg[0], sample.accel_mg[1],
               sample.accel_mg[2], sample.gyro_dps[0], sample.gyro_dps[1],
               sample.gyro_dps[2], sample.temperature_c);
      }

      if ((sample_count % LED_TOGGLE_SAMPLE_COUNT) == 0) {
        led_on = !led_on;
        if (led_on) {
          set_status_led(0, STATUS_LED_BRIGHTNESS, 0);
        } else {
          ESP_ERROR_CHECK(led_strip_clear(status_led));
        }
      }
    } else {
      ESP_LOGE(TAG, "ICM-45686 read failed: %s", esp_err_to_name(error));
      set_status_led(STATUS_LED_BRIGHTNESS, 0, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(IMU_SAMPLE_PERIOD_MS));
  }
}
