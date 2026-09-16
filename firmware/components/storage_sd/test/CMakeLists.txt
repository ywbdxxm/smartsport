cmake_minimum_required(VERSION 3.16)

project(storage_sd_host_tests C)

enable_testing()

add_executable(
  storage_sd_pattern_test
  ../storage_sd_pattern.c
  test_storage_sd_pattern.c
)
target_include_directories(storage_sd_pattern_test PRIVATE ../private_include)

if(MSVC)
  target_compile_options(storage_sd_pattern_test PRIVATE /W4 /WX)
else()
  target_compile_options(storage_sd_pattern_test PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME storage_sd_pattern COMMAND storage_sd_pattern_test)
