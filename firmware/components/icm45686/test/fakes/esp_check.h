#pragma once

#include "esp_err.h"

#define ESP_RETURN_ON_FALSE(condition, error, tag, format, ...)                \
  do {                                                                         \
    (void)(tag);                                                               \
    if (!(condition)) {                                                        \
      return (error);                                                          \
    }                                                                          \
  } while (0)

#define ESP_RETURN_ON_ERROR(expression, tag, format, ...)                      \
  do {                                                                         \
    (void)(tag);                                                               \
    esp_err_t esp_check_error_ = (expression);                                 \
    if (esp_check_error_ != ESP_OK) {                                          \
      return esp_check_error_;                                                 \
    }                                                                          \
  } while (0)
