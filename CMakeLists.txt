cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(picoThread C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
pico_sdk_init()

add_executable(test
  thread.cpp
)
pico_enable_stdio_usb(test 1)
pico_enable_stdio_uart(test 0)
pico_add_extra_outputs(test)
target_link_libraries(test pico_stdlib pico_multicore pico_sync)
