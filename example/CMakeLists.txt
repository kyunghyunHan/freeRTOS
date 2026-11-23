cmake_minimum_required(VERSION 3.24)
project(f429_native_examples C ASM)
set(APP "example01" CACHE STRING "example01 through example25")
if(NOT APP MATCHES "^example(0[1-9]|1[0-9]|2[0-5])$")
  message(FATAL_ERROR "APP must be example01 through example25")
endif()
string(SUBSTRING "${APP}" 7 2 number)
string(REGEX REPLACE "^0" "" number "${number}")
get_filename_component(ROOT ".." ABSOLUTE)
set(KERNEL "${ROOT}/external/FreeRTOS-Kernel")
set(DEVICE "${ROOT}/external/cmsis-device-f4")
set(CORE "${ROOT}/external/CMSIS_5/CMSIS/Core/Include")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(firmware
  "${APP}.c"
  "${DEVICE}/Source/Templates/system_stm32f4xx.c"
  "${DEVICE}/Source/Templates/gcc/startup_stm32f429xx.s"
  "${KERNEL}/tasks.c" "${KERNEL}/queue.c" "${KERNEL}/list.c"
  "${KERNEL}/timers.c" "${KERNEL}/event_groups.c"
  "${KERNEL}/portable/GCC/ARM_CM4F/port.c"
  "${KERNEL}/portable/MemMang/heap_4.c")
set_target_properties(firmware PROPERTIES SUFFIX ".elf" C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_include_directories(firmware PRIVATE . "${KERNEL}/include"
  "${KERNEL}/portable/GCC/ARM_CM4F" "${DEVICE}/Include" "${CORE}")
target_compile_definitions(firmware PRIVATE STM32F429xx HSI_VALUE=16000000U EXAMPLE_NUMBER=${number})
target_compile_options(firmware PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
  -mfloat-abi=hard -ffunction-sections -fdata-sections -fno-common -Wall -Wextra -Werror -g3)
target_link_options(firmware PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  -nostartfiles "-T${ROOT}/cmake/stm32f429zi.ld" -Wl,--gc-sections
  -Wl,-z,noexecstack -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map")
set_property(TARGET firmware APPEND PROPERTY LINK_DEPENDS "${ROOT}/cmake/stm32f429zi.ld")
add_custom_command(TARGET firmware POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O ihex $<TARGET_FILE:firmware> "${CMAKE_BINARY_DIR}/firmware.hex"
  COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:firmware> "${CMAKE_BINARY_DIR}/firmware.bin"
  COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:firmware>)
if(EXAMPLE_NEWLIB)
  target_sources(firmware PRIVATE newlib_init.c)
  target_link_options(firmware PRIVATE --specs=nano.specs --specs=nosys.specs)
endif()
