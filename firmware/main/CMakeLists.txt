idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES esp_driver_i2c esp_psram icm45686 spi_flash
                       INCLUDE_DIRS "")
