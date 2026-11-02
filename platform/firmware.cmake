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
add_library(platform STATIC
  "${FREERTOS_ROOT}/platform/board.c"
  "${FREERTOS_ROOT}/platform/hooks.c"
  "${FREERTOS_ROOT}/platform/runtime.c"
  "${DEVICE}/Source/Templates/system_stm32f4xx.c"
  "${KERNEL}/tasks.c" "${KERNEL}/queue.c" "${KERNEL}/list.c"
  "${KERNEL}/portable/GCC/ARM_CM4F/port.c"
  "${KERNEL}/portable/MemMang/heap_4.c")
target_include_directories(platform PUBLIC "${FREERTOS_ROOT}/platform" "${KERNEL}/include"
  "${KERNEL}/portable/GCC/ARM_CM4F" "${DEVICE}/Include" "${CORE}")
target_compile_definitions(platform PUBLIC STM32F446xx HSI_VALUE=16000000U)
target_compile_options(platform PUBLIC -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  -ffunction-sections -fdata-sections -fno-common -Wall -Wextra -g3)
target_compile_options(platform PRIVATE -Werror)
add_executable(firmware "${DEVICE}/Source/Templates/gcc/startup_stm32f446xx.s")
target_link_libraries(firmware PRIVATE platform)
target_link_options(firmware PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  -nostartfiles "-T${FREERTOS_ROOT}/platform/stm32f446re.ld"
  -Wl,--gc-sections -Wl,-z,noexecstack -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map")
set_property(TARGET firmware APPEND PROPERTY LINK_DEPENDS "${FREERTOS_ROOT}/platform/stm32f446re.ld")
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
  target_sources(firmware PRIVATE "${FREERTOS_ROOT}/platform/rust_entry.c"
    "${FREERTOS_ROOT}/platform/rust_bridge.c")
  target_link_libraries(firmware PRIVATE "${rust_archive}" platform)
endfunction()
