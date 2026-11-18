# Optional compatibility wrapper. Cargo owns compilation and final ELF linking.
set(BOARD "nucleo_f429zi" CACHE STRING "Target board")
if(NOT BOARD STREQUAL "nucleo_f429zi")
  message(FATAL_ERROR "This Rust application targets nucleo_f429zi; use a new build/nucleo_f429zi directory")
endif()
find_program(CARGO_EXECUTABLE cargo HINTS "$ENV{HOME}/.cargo/bin" REQUIRED)
if(NOT CMAKE_OBJCOPY)
  find_program(CMAKE_OBJCOPY NAMES arm-zephyr-eabi-objcopy arm-none-eabi-objcopy
    HINTS "$ENV{HOME}/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin" REQUIRED)
endif()
set(cargo_output "${CMAKE_BINARY_DIR}/rust/target")
set(cargo_elf "${cargo_output}/thumbv7em-none-eabihf/release/firmware")
add_custom_target(firmware ALL
  COMMAND "${CARGO_EXECUTABLE}" build --release --target thumbv7em-none-eabihf
    --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
    --target-dir "${cargo_output}" --locked --offline
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${cargo_elf}" "${CMAKE_BINARY_DIR}/firmware.elf"
  COMMAND "${CMAKE_OBJCOPY}" -O ihex "${cargo_elf}" "${CMAKE_BINARY_DIR}/firmware.hex"
  COMMAND "${CMAKE_OBJCOPY}" -O binary "${cargo_elf}" "${CMAKE_BINARY_DIR}/firmware.bin"
  BYPRODUCTS "${CMAKE_BINARY_DIR}/firmware.elf" "${CMAKE_BINARY_DIR}/firmware.hex"
    "${CMAKE_BINARY_DIR}/firmware.bin"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM USES_TERMINAL)
