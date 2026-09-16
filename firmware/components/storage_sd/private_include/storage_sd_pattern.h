#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void storage_sd_pattern_fill(uint8_t *buffer, size_t length, size_t offset);

bool storage_sd_pattern_matches(const uint8_t *buffer, size_t length,
                                size_t offset, size_t *first_bad_offset);
