# SD Storage Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an owned SDSPI/FAT component and a safe 32 KiB boot smoke test without making SD availability a prerequisite for IMU sampling.

**Architecture:** `storage_sd` exclusively owns SPI2, the mounted card, and cleanup behind an opaque handle. Platform-neutral pattern generation is host-tested; the application performs one mount/test/unmount attempt before entering its existing IMU wait-and-sample flow.

**Tech Stack:** C17-compatible C, ESP-IDF 6.1 SDSPI/FAT APIs, CMake/CTest host tests, ESP32-S3.

**Spec:** `docs/superpowers/specs/2026-09-16-sensor-recording-architecture-design.md`

## Global Constraints

- Target is `esp32s3` with ESP-IDF 6.1 from `C:\esp\v6.1\esp-idf` and its matching EIM activation profile.
- SD uses SPI2 at 10 MHz: GPIO10 CS, GPIO11 MOSI, GPIO12 SCLK, GPIO13 MISO; GPIO14 is observed as raw card-detect input.
- FAT mounts at `/sdcard` and `format_if_mount_failed` is always `false`.
- SD failure is logged and must not prevent the existing IMU initialization and sampling loop.
- This increment does not add the recorder queue, a persistent session format, GNSS code, or heart-rate code.
- Physical SD verification is skipped while the board is disconnected and must be reported as skipped.

---

### Task 1: Deterministic smoke-test data

**Files:**
- Create: `firmware/components/storage_sd/private_include/storage_sd_pattern.h`
- Create: `firmware/components/storage_sd/storage_sd_pattern.c`
- Create: `firmware/components/storage_sd/test/CMakeLists.txt`
- Create: `firmware/components/storage_sd/test/test_storage_sd_pattern.c`

**Interfaces:**
- Produces: `storage_sd_pattern_fill(uint8_t *buffer, size_t length, size_t offset)`
- Produces: `storage_sd_pattern_matches(const uint8_t *buffer, size_t length, size_t offset, size_t *first_bad_offset)`

- [x] **Step 1: Write the failing host test and build description**

The test must use the literal first bytes `5A 79 98 B7 D6 F5 14 33`, verify a buffer generated from offset 509, verify an unmodified buffer, then corrupt byte 17 and require `first_bad_offset == 526`.

- [x] **Step 2: Run the host build and prove RED**

Run from `firmware/components/storage_sd/test` in the activated IDF 6.1 shell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Expected: the test compiles and the link fails with unresolved `storage_sd_pattern_fill` and `storage_sd_pattern_matches` because no production implementation is linked yet.

- [x] **Step 3: Add the minimal implementation**

Generate byte `n` as `(uint8_t)(0x5A + 31u * (offset + n))`. Verification must report the absolute offset of the first mismatch and accept a null `first_bad_offset`.

- [x] **Step 4: Prove GREEN**

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Expected: one test passes with zero failures and strict warnings enabled.

### Task 2: Owned SD mount and smoke-test component

**Files:**
- Create: `firmware/components/storage_sd/CMakeLists.txt`
- Create: `firmware/components/storage_sd/README.md`
- Create: `firmware/components/storage_sd/include/storage_sd.h`
- Create: `firmware/components/storage_sd/storage_sd.c`

**Interfaces:**
- Consumes: the two pattern functions from Task 1.
- Produces: opaque `storage_sd_handle_t`.
- Produces: `esp_err_t storage_sd_mount(const storage_sd_config_t *config, storage_sd_handle_t *out_handle)`.
- Produces: `esp_err_t storage_sd_run_smoke_test(storage_sd_handle_t handle)`.
- Produces: `esp_err_t storage_sd_unmount(storage_sd_handle_t handle)`; the handle is consumed even when cleanup reports an error.

- [x] **Step 1: Define the narrow public API and ownership contract**

`storage_sd_config_t` contains the SPI host, MOSI, MISO, SCLK, CS, card-detect GPIO, and maximum clock in kHz. The public header defines `/sdcard` as the stable mount point and does not expose ESP-IDF card or filesystem handles.

- [x] **Step 2: Implement mount with complete failure cleanup**

Configure GPIO14 as input without internal pulls and log its raw value. Initialize the configured SPI bus, use `SDSPI_HOST_DEFAULT()` with the configured host and 10000 kHz limit, leave `slot_config.gpio_cd` disabled, and mount with four open files, 16 KiB allocation units, and formatting disabled. On failure, release the bus and heap context before returning.

- [x] **Step 3: Implement exact 32 KiB smoke behavior**

Use 512-byte chunks. Write deterministic bytes, flush, `fsync`, close, reopen, verify every byte plus EOF at exactly 32768 bytes, close, and remove only `/sdcard/smartsport_sd_test.bin`. All exits close any open stream and attempt to remove this owned test path.

- [x] **Step 4: Implement lifecycle cleanup and documentation**

Unmount FAT/SDSPI, free SPI2, free the opaque context exactly once, and return the first cleanup error. Document 3.3 V-only power, exclusive bus ownership, non-calibrated card-detect polarity, non-formatting behavior, and the caller's single-thread ownership obligation.

### Task 3: Board and application integration

**Files:**
- Modify: `firmware/main/board_config.h`
- Modify: `firmware/main/CMakeLists.txt`
- Modify: `firmware/main/hello_world_main.c`
- Modify: `firmware/README.md`
- Modify: `README.md`
- Create: `doc/数据采集与存储架构.md`

**Interfaces:**
- Consumes: the `storage_sd` public API from Task 2.
- Preserves: existing ICM-45686 sample rate, one-second log throttle, and LED behavior.

- [x] **Step 1: Add board-owned SD policy**

Add SPI2 and GPIO10-14 constants, 10000 kHz, and no pin literals in `storage_sd` or application logic.

- [x] **Step 2: Add one non-fatal boot smoke attempt**

After status-LED initialization and before I2C/IMU initialization, mount, test, and unmount. Log each returned error. Always continue into the existing IMU path.

- [x] **Step 3: Update build dependencies and operator documentation**

Make `main` privately require `storage_sd`. Document the smoke-test sequence, exact diagnostic path, non-format guarantee, expected logs, and the commands for the later COM-port hardware test.

### Task 4: Verification, review, and delivery

**Files:**
- Modify: this plan to mark completed steps.

**Interfaces:**
- Consumes: all earlier tasks.
- Produces: a verified commit on `main` and an updated `origin/main`.

- [x] **Step 1: Run all host tests**

Configure/build/test `firmware/components/icm45686/test`, `firmware/main/test`, and `firmware/components/storage_sd/test` with the activated ESP-IDF 6.1 tool environment. Expected total: four tests, zero failures.

- [x] **Step 2: Run a clean target configure and build**

From `firmware`, activate the selected EIM profile and run:

```powershell
idf.py reconfigure
idf.py build
```

Expected: ESP32-S3 firmware and bootloader build successfully with the checked-in dependency lock.

- [x] **Step 3: Run static repository checks and self-review**

Run `git diff --check`, inspect the full diff, verify `format_if_mount_failed = false`, verify SD errors cannot bypass IMU startup, and confirm only the diagnostic file can be removed.

- [x] **Step 4: Commit and push the requested main branch**

Commit the architecture/plan separately from implementation if the history is not already split, commit implementation with a focused message, and push `main:main` without changing global proxy configuration.

- [x] **Step 5: Record the hardware skip**

Report that no-card/card-detect/write/readback behavior remains unverified because the board is disconnected. The next physical test uses a backed-up FAT card and the documented 3.3 V wiring.
