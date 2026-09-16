cmake_minimum_required(VERSION 3.16)

project(imu_log_throttle_host_test C)

enable_testing()

add_executable(
  imu_log_throttle_test
  ../imu_log_throttle.c
  test_imu_log_throttle.c
)
target_include_directories(imu_log_throttle_test PRIVATE ..)

if(MSVC)
  target_compile_options(imu_log_throttle_test PRIVATE /W4 /WX)
else()
  target_compile_options(imu_log_throttle_test PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME imu_log_throttle COMMAND imu_log_throttle_test)
