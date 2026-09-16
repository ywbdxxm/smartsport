# Smart Sport Sensor Recording Architecture Design

**Date:** 2026-09-16

**Status:** Approved for incremental implementation. This increment delivers the SD storage boundary and a destructive-scope-limited card smoke test; it does not yet implement session recording or the GNSS and heart-rate drivers.

## Product goal

The vest must collect IMU, GNSS, and heart-rate data without letting slow or missing storage stop sensor acquisition. Data from different sources must share one time model and one durable session format so an offline tool can reconstruct a workout and export it to CSV.

## Fixed platform constraints

- Target module: ESP32-S3-WROOM-1-N16R8 on the Xinlu City ESP32-S3 development board.
- SDK: ESP-IDF 6.1; target `esp32s3`.
- Memory: 16 MB flash and 8 MB Octal PSRAM at 80 MHz.
- IMU: JYTech ICM-45686 on I2C0, GPIO8 SDA and GPIO9 SCL, currently polled every 100 ms.
- SD: Waveshare Micro SD Storage Board SKU 3947 on SPI2, GPIO10 CS, GPIO11 MOSI, GPIO12 SCLK, GPIO13 MISO, and GPIO14 card-detect observation.
- SD voltage is 3.3 V only. Initial SPI clock is 10 MHz.
- GNSS UART/PPS GPIOs are reserved, but its carrier-board electrical identity is not yet verified. Heart-rate hardware and protocol are not yet selected.
- The firmware must never format a card automatically.

The authoritative wiring and electrical evidence remains `doc/模块接线最终规划.md`.

## System boundaries and ownership

The eventual recording path has four layers:

1. Sensor drivers own their peripheral transactions and translate raw device values into typed samples.
2. Sensor producer tasks timestamp samples with the ESP32 monotonic clock and submit fixed-size record messages. Producers never open files or call an SD API.
3. A single recorder task owns the record queue consumer, session state, serialization buffer, SD mount handle, and session file handle. No other task writes the session file.
4. The `storage_sd` component owns the SPI2 bus lifecycle and FAT mount. It exposes mount, smoke-test, mount-point, and unmount operations without exposing the `sdmmc_card_t` object.

The first increment implements layer 4 only. It is intentionally usable before the GNSS and heart-rate protocols are fixed.

## Data flow and backpressure

Each producer submits a record through a non-blocking API. The initial recorder queue will hold 512 fixed-size record messages. At an estimated 64 bytes per message, this consumes about 32 KiB and holds roughly 2.5 seconds at a 200 Hz aggregate input rate.

When the queue is full, a producer drops the new record rather than blocking sensor acquisition. Each source keeps a dropped-record counter, and the recorder writes a health record containing per-source drops and the queue high-water mark after capacity becomes available. This policy preserves ordering of already accepted records and makes overload visible.

The recorder batches sequential records and synchronizes the file at least once per second, at a session stop, and after a critical health event. A power loss may therefore lose the newest unsynchronized interval, but earlier complete records remain parseable.

## Time model

Every record carries a 64-bit monotonic timestamp in microseconds. Monotonic time is the ordering and duration authority and is never replaced by wall time.

When GNSS supplies valid UTC, the GNSS producer emits a time-mapping record containing the monotonic timestamp, UTC value, and validity/quality fields. Offline software derives UTC for other records from these mappings. A loss or correction of GNSS time cannot reorder samples.

## Session and record format direction

Each workout creates one binary session file. The file begins with a versioned session header containing a format identifier, firmware/build identity, board identity, session sequence, and sensor configuration. Each record envelope contains:

- record format version;
- record type;
- payload length;
- per-session sequence number;
- monotonic timestamp in microseconds;
- typed payload;
- CRC covering the envelope and payload.

The parser stops cleanly at the first incomplete or CRC-invalid trailing record. The exact byte layout and sensor payload schemas are deliberately excluded from this increment: they will be fixed in the recorder implementation spec after the GNSS carrier and heart-rate device identities are known. This does not affect the SD component API.

Session filenames use a monotonic boot/session sequence rather than assuming UTC is available at startup. UTC mappings live inside the file. A separate host utility will validate records and export typed CSV files without making CSV the on-device storage format.

## SD component contract

`storage_sd` has one caller-owned opaque handle and one lifecycle:

```text
unmounted -> storage_sd_mount() -> mounted -> storage_sd_unmount() -> released
```

The component:

- exclusively owns the configured SPI host from successful bus initialization until unmount;
- mounts FAT at `/sdcard` with `format_if_mount_failed = false`;
- observes and logs the raw GPIO14 card-detect level but does not use it to block SDSPI initialization until its installed-card polarity is measured;
- reports card identity, type, capacity, and negotiated frequency;
- releases the SDSPI device, FAT registration, SPI bus, and heap context on normal unmount;
- leaves sensor startup independent of every SD error.

The startup smoke test writes exactly 32 KiB of deterministic data to `/sdcard/smartsport_sd_test.bin`, calls `fflush()` and `fsync()`, closes the file, reopens it, verifies every byte and the exact file length, then removes only that test file. It never formats, renames, scans, or deletes user files.

## Startup behavior for this increment

At boot, after the status LED is available and before waiting for the IMU, the application performs one SD mount/smoke/unmount attempt. Success and each failure stage are logged. A missing card, incompatible filesystem, write error, verification error, or cleanup error is non-fatal: the firmware proceeds to IMU initialization and sampling.

This ordering allows SD testing even when the IMU is disconnected and proves that storage failure is not a prerequisite for sensor operation.

## Error handling and observability

- Public functions reject null pointers and invalid lifecycle use with `ESP_ERR_INVALID_ARG`.
- Mount failures release resources acquired by that call.
- Standard-I/O failures include the operation and `errno` in the log but return an `esp_err_t` boundary value.
- The card-detect log explicitly says the polarity is not yet calibrated.
- No loop retries SD indefinitely during boot.
- No sensitive payload or user file contents are logged.

## Verification strategy

The deterministic data generator and verifier are platform-neutral and receive host tests that prove an independently calculated byte sequence, success at non-zero offsets, and absolute mismatch reporting.

The target-dependent layer is compiled against the exact ESP-IDF 6.1 / ESP32-S3 tuple. Physical validation, once the board is reconnected, consists of: no-card boot, FAT card mount, 32 KiB write/sync/readback/delete, raw card-detect levels with and without a card, and confirmation that IMU startup proceeds after an SD failure.

Build success is not evidence of SD electrical behavior, filesystem compatibility, card-detect polarity, or power-loss durability.

## Compatibility impact

This increment adds a component and board pin constants. It does not change partition tables, NVS keys, OTA identity, IMU behavior, or any persisted product data format. The smoke-test filename is reserved for firmware diagnostics and is not a session-file contract.
