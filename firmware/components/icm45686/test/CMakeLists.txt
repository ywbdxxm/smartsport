cmake_minimum_required(VERSION 3.16)

project(icm45686_host_tests C)

enable_testing()

function(enable_strict_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endfunction()

add_executable(
  icm45686_conversion_test
  ../icm45686_conversion.c
  test_icm45686_conversion.c
)
target_include_directories(
  icm45686_conversion_test
  PRIVATE ../include ../private_include
)
enable_strict_warnings(icm45686_conversion_test)
add_test(NAME icm45686_conversion COMMAND icm45686_conversion_test)

add_executable(
  icm45686_driver_test
  ../icm45686.c
  ../icm45686_conversion.c
  test_icm45686_driver.c
)
target_include_directories(
  icm45686_driver_test
  BEFORE PRIVATE fakes ../include ../private_include
)
enable_strict_warnings(icm45686_driver_test)
add_test(NAME icm45686_driver COMMAND icm45686_driver_test)
