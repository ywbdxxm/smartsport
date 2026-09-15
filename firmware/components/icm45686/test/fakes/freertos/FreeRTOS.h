#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(delay_ms) ((TickType_t)(delay_ms))
