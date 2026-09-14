/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>

#define STATUS_LED_GPIO 48
#define STATUS_LED_COUNT 1
#define STATUS_LED_RMT_RESOLUTION_HZ (10 * 1000 * 1000)
#define STATUS_LED_HALF_PERIOD_MS 500
#define STATUS_LED_BRIGHTNESS 16

static led_strip_handle_t status_led;

static void configure_status_led(void) {
  const led_strip_config_t strip_config = {
      .strip_gpio_num = STATUS_LED_GPIO,
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
  printf("Blinking onboard WS2812B on GPIO %d.\n", STATUS_LED_GPIO);

  uint32_t blink_count = 0;
  while (1) {
    ESP_ERROR_CHECK(led_strip_set_pixel(
        status_led, 0, STATUS_LED_BRIGHTNESS, STATUS_LED_BRIGHTNESS,
        STATUS_LED_BRIGHTNESS));
    ESP_ERROR_CHECK(led_strip_refresh(status_led));
    vTaskDelay(pdMS_TO_TICKS(STATUS_LED_HALF_PERIOD_MS));

    ESP_ERROR_CHECK(led_strip_clear(status_led));
    vTaskDelay(pdMS_TO_TICKS(STATUS_LED_HALF_PERIOD_MS));

    printf("LED blink count: %" PRIu32 "\n", ++blink_count);
  }
}
