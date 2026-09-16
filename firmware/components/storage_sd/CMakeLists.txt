idf_component_register(
    SRCS "storage_sd.c" "storage_sd_pattern.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES esp_driver_gpio esp_driver_spi fatfs
)
