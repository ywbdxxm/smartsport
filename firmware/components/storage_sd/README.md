# SD storage component

This component owns one configured SPI host, the SDSPI card device, and the FAT mount at `/sdcard`. Its handle is single-owner and must be used from one caller context; `storage_sd_unmount()` consumes the handle even when a cleanup operation reports an error.

The current board configuration uses the Waveshare Micro SD Storage Board (SKU 3947) at 3.3 V only. SPI2 starts at 10 MHz. GPIO14 card detect is configured as a plain input and its raw level is logged, but the level is not used to accept or reject a mount until installed-card polarity is measured on hardware.

Mounting never formats a card. `storage_sd_run_smoke_test()` creates `/sdcard/smartsport_sd_test.bin`, writes and synchronizes exactly 32 KiB, closes and reads it back byte-for-byte, verifies the exact length, then removes only that diagnostic file. A failed smoke test also attempts to remove the partial diagnostic file.

The component does not own workout sessions yet. The future recorder task will be the only long-lived caller holding the mounted handle and session file.

## Host test

The deterministic byte pattern is platform-neutral:

```powershell
cmake -S test -B test/build
cmake --build test/build --config Debug
ctest --test-dir test/build -C Debug --output-on-failure
```
