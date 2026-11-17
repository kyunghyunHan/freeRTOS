cmake_minimum_required(VERSION 3.24)
project(c_gpio_button C ASM)
include(../../platform/firmware.cmake)
target_sources(firmware PRIVATE src/main.c)
