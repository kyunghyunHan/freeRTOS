cmake_minimum_required(VERSION 3.24)
project(c_led_blink C ASM)
include(../../platform/firmware.cmake)
target_sources(firmware PRIVATE src/main.c)
