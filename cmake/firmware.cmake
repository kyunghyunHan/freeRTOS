get_filename_component(FREERTOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXECUTABLE_SUFFIX .elf)
set(KERNEL "${FREERTOS_ROOT}/external/FreeRTOS-Kernel")
set(DEVICE "${FREERTOS_ROOT}/external/cmsis-device-f4")
set(CORE "${FREERTOS_ROOT}/external/CMSIS_5/CMSIS/Core/Include")
foreach(required "${KERNEL}/tasks.c" "${DEVICE}/Include/stm32f446xx.h" "${CORE}/core_cm4.h")
  if(NOT EXISTS "${required}")
    message(FATAL_ERROR "Dependency missing: ${required}. See docs/dependencies.md")
  endif()
endforeach()
# 공통 설정, 프로젝트별 재정의 가능
set(FIRMWARE_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "FreeRTOSConfig.h directory")
set(FIRMWARE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/stm32f446re.ld" CACHE FILEPATH "Linker script")
set(FIRMWARE_EXTRA_INCLUDE_DIRS "" CACHE STRING "Additional include directories")
set(FIRMWARE_EXTRA_SOURCES "" CACHE STRING "Additional source files")
set(FIRMWARE_EXTRA_DEFINITIONS "" CACHE STRING "Additional compile definitions")
foreach(required "${FIRMWARE_CONFIG_DIR}/FreeRTOSConfig.h" "${FIRMWARE_LINKER_SCRIPT}")
  if(NOT EXISTS "${required}")
    message(FATAL_ERROR "Configuration missing: ${required}")
  endif()
endforeach()
add_library(freertos_kernel STATIC
  "${DEVICE}/Source/Templates/system_stm32f4xx.c"
  "${KERNEL}/tasks.c" "${KERNEL}/queue.c" "${KERNEL}/list.c"
  "${KERNEL}/portable/GCC/ARM_CM4F/port.c"
  "${KERNEL}/portable/MemMang/heap_4.c")
target_include_directories(freertos_kernel PUBLIC ${FIRMWARE_EXTRA_INCLUDE_DIRS}
  "${FIRMWARE_CONFIG_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" "${KERNEL}/include"
  "${KERNEL}/portable/GCC/ARM_CM4F" "${DEVICE}/Include" "${CORE}")
target_compile_definitions(freertos_kernel PUBLIC STM32F446xx HSI_VALUE=16000000U
  ${FIRMWARE_EXTRA_DEFINITIONS})
target_compile_options(freertos_kernel PUBLIC -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  -ffunction-sections -fdata-sections -fno-common -Wall -Wextra -g3)
target_compile_options(freertos_kernel PRIVATE -Werror)
# 기본 HardFault가 startup의 weak 별칭보다 먼저 연결되도록 배치
add_executable(firmware "${FREERTOS_ROOT}/cmake/freertos_hooks.c"
  "${FREERTOS_ROOT}/cmake/freertos_runtime.c"
  "${DEVICE}/Source/Templates/gcc/startup_stm32f446xx.s" ${FIRMWARE_EXTRA_SOURCES})
target_compile_options(firmware PRIVATE -Werror)
target_link_libraries(firmware PRIVATE freertos_kernel)
target_link_options(firmware PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  -nostartfiles "-T${FIRMWARE_LINKER_SCRIPT}" -Wl,--wrap=vTaskStartScheduler
  -Wl,--gc-sections -Wl,-z,noexecstack -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map")
set_property(TARGET firmware APPEND PROPERTY LINK_DEPENDS "${FIRMWARE_LINKER_SCRIPT}")
add_custom_command(TARGET firmware POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O ihex $<TARGET_FILE:firmware> "${CMAKE_BINARY_DIR}/firmware.hex"
  COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:firmware> "${CMAKE_BINARY_DIR}/firmware.bin"
  COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:firmware>)

# Cargo compiles only the no_std application. The C kernel is linked exactly once.
function(add_rust_application)
  find_program(CARGO_EXECUTABLE cargo HINTS "/Users/hangyeonghyeon/.cargo/bin" REQUIRED)
  set(rust_out "${CMAKE_BINARY_DIR}/rust/target")
  set(rust_archive "${rust_out}/thumbv7em-none-eabihf/debug/librustapp.a")
  # Ninja creates output parents before Cargo. Mark this dedicated cache so
  # Cargo 1.99 can safely validate --target-dir during cargo clean.
  file(WRITE "${CMAKE_BINARY_DIR}/cargo-cache.tag"
    "Signature: 8a477f597d28d172789f06886806bc55\n# Dedicated Cargo build cache.\n")
  add_custom_target(rust_application
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${rust_out}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_BINARY_DIR}/cargo-cache.tag" "${rust_out}/CACHEDIR.TAG"
    COMMAND "${CARGO_EXECUTABLE}" build --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
      --target thumbv7em-none-eabihf --target-dir "${rust_out}" --locked --offline
    BYPRODUCTS "${rust_archive}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM USES_TERMINAL)
  # Always ask Cargo to check freshness, including path dependencies and deleted outputs.
  add_dependencies(firmware rust_application)
  target_sources(firmware PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
  target_link_libraries(firmware PRIVATE "${rust_archive}" freertos_kernel)
endfunction()
