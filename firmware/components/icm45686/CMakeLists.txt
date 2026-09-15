idf_component_register(
    SRCS
        "icm45686.c"
        "icm45686_conversion.c"
        "vendor/inv_imu_driver.c"
        "vendor/inv_imu_transport.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include" "vendor"
    REQUIRES esp_driver_i2c
)
