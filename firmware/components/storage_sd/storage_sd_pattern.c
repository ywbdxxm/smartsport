#include "storage_sd_pattern.h"

static uint8_t pattern_byte(size_t offset) {
  return (uint8_t)(0x5AU + 31U * offset);
}

void storage_sd_pattern_fill(uint8_t *buffer, size_t length, size_t offset) {
  for (size_t index = 0; index < length; ++index) {
    buffer[index] = pattern_byte(offset + index);
  }
}

bool storage_sd_pattern_matches(const uint8_t *buffer, size_t length,
                                size_t offset, size_t *first_bad_offset) {
  for (size_t index = 0; index < length; ++index) {
    if (buffer[index] != pattern_byte(offset + index)) {
      if (first_bad_offset != NULL) {
        *first_bad_offset = offset + index;
      }
      return false;
    }
  }
  return true;
}
