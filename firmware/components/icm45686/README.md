# ICM-45686 ESP-IDF component

This component adapts the JYTech ICM-45686 pure sensor module to the ESP-IDF I2C master driver.

- Interface: I2C at 400 kHz
- Address: auto-detect 0x69 first, then 0x68
- Power-up: wait 3 ms and retry the first probe once for each address
- Configuration: accelerometer ±4 g, gyroscope ±1000 dps, both at 200 Hz in low-noise mode
- Output: acceleration in mg, angular velocity in degrees/s, temperature in degrees C
- Acquisition: polling the sensor data registers; FIFO and INT1/INT2 are not enabled yet

The files under `vendor/` are InvenSense basic driver 2.1.0 copied from the project’s pinned `ICM45686-Module` source at commit `2d7ae7a76d0b7754bb4a3c20276d078c3f146974`. Their original permissive license headers are preserved. The transport callback signatures have one local change: an application context pointer was added so each ESP-IDF sensor handle owns its I2C device without global mutable transport state.

Host tests use CMake and CTest. From the `firmware` directory, run them in a terminal configured with a host C compiler:

```powershell
cmake -S components/icm45686/test -B build/icm45686-host-tests
cmake --build build/icm45686-host-tests --config Debug
ctest --test-dir build/icm45686-host-tests -C Debug --output-on-failure
```

The tests cover unit conversion, initial NACK retry, address fallback, WHO_AM_I validation, timeout propagation, and device cleanup. They do not replace testing with the physical module.
