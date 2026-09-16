#include "storage_sd_pattern.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #condition);                                                     \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int test_fill_uses_expected_sequence(void) {
  static const uint8_t expected[] = {0x5A, 0x79, 0x98, 0xB7,
                                     0xD6, 0xF5, 0x14, 0x33};
  uint8_t actual[sizeof(expected)] = {0};

  storage_sd_pattern_fill(actual, sizeof(actual), 0);

  CHECK(memcmp(actual, expected, sizeof(expected)) == 0);
  return 0;
}

static int test_fill_respects_absolute_offset(void) {
  static const uint8_t expected[] = {0xFD, 0x1C, 0x3B, 0x5A};
  uint8_t actual[sizeof(expected)] = {0};

  storage_sd_pattern_fill(actual, sizeof(actual), 509);

  CHECK(memcmp(actual, expected, sizeof(expected)) == 0);
  return 0;
}

static int test_match_reports_absolute_bad_offset(void) {
  uint8_t data[32] = {0};
  size_t first_bad_offset = 0;

  storage_sd_pattern_fill(data, sizeof(data), 509);
  CHECK(storage_sd_pattern_matches(data, sizeof(data), 509, NULL));

  data[17] ^= 0x01;
  CHECK(
      !storage_sd_pattern_matches(data, sizeof(data), 509, &first_bad_offset));
  CHECK(first_bad_offset == 526);
  return 0;
}

int main(void) {
  CHECK(test_fill_uses_expected_sequence() == 0);
  CHECK(test_fill_respects_absolute_offset() == 0);
  CHECK(test_match_reports_absolute_bad_offset() == 0);
  puts("SD smoke-test pattern host tests passed");
  return 0;
}
