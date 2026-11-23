# CubeIDE: File > New > Makefile Project with Existing Code, directory=example.
# ARM_GCC_BIN: STM32CubeIDE GNU tools/bin, 또는 PATH에 arm-none-eabi-gcc 등록.
APP ?= example01
BUILD_DIR ?= build/cubeide/$(APP)
ARM_GCC_BIN ?=
CMAKE ?= cmake
.PHONY: all configure clean
all: configure
	"$(CMAKE)" --build "$(BUILD_DIR)"
configure:
	"$(CMAKE)" -S . -B "$(BUILD_DIR)" -G Ninja -DAPP=$(APP) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$(CURDIR)/arm-none-eabi.cmake" -DARM_GCC_BIN="$(ARM_GCC_BIN)"
clean:
	"$(CMAKE)" --build "$(BUILD_DIR)" --target clean
