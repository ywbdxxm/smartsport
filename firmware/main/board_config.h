#pragma once

/* Xinlu City ESP32S3 board with ESP32-S3-WROOM-1-N16R8. */
#define BOARD_STATUS_LED_GPIO 48

/* JYTech ICM-45686 module I2C wiring. */
#define BOARD_IMU_INT1_GPIO 7
#define BOARD_IMU_I2C_PORT 0
#define BOARD_IMU_I2C_SDA_GPIO 8
#define BOARD_IMU_I2C_SCL_GPIO 9
#define BOARD_IMU_I2C_CLOCK_HZ 400000

/* Waveshare Micro SD Storage Board on the dedicated SPI2 bus. */
#define BOARD_SD_SPI_HOST SPI2_HOST
#define BOARD_SD_CS_GPIO 10
#define BOARD_SD_MOSI_GPIO 11
#define BOARD_SD_SCLK_GPIO 12
#define BOARD_SD_MISO_GPIO 13
#define BOARD_SD_CD_GPIO 14
#define BOARD_SD_MAX_FREQ_KHZ 10000
