# 환경 설정

macOS Apple Silicon 기준입니다. 아래 도구가 설치되어 있습니다(2026-11-03).

| 도구 | 버전 | 경로 |
|---|---|---|
| CMake / Ninja | 4.4.3 / 1.13.2 | /opt/homebrew/bin |
| OpenOCD | 0.12.0 | /opt/homebrew/bin/openocd |
| GCC | 14.3.0 | ~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc |
| GDB | 16.2 | 같은 SDK bin/arm-zephyr-eabi-gdb |
| Rust / Cargo | 1.99.0 / 1.99.0 | ~/.cargo/bin |
| clangd | — | /opt/homebrew/opt/llvm/bin/clangd |

CMake의 `platform/arm-gcc.cmake`는 위 SDK 경로를 사용합니다.
Homebrew ARM GCC 16.2.0에는 libc가 없어 SDK의 GCC와 picolibc를 사용합니다.
SDK 설치 위치를 바꾸면 toolchain 파일과 `.vscode`의 GCC/GDB 경로도 수정하세요.
`ARM_TOOLCHAIN_BIN`은 같은 `arm-zephyr-eabi` prefix를 쓰는 다른 bin 경로로 지정할 수 있습니다.

## 환경 확인

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
command -v cmake ninja openocd cargo rustc
cmake --version
ninja --version
openocd --version
rustc -Vv
cargo -V
rustup target list --installed
/Users/hangyeonghyeon/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc --version
/Users/hangyeonghyeon/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb --version
```

## 새 환경 설치

이미 설치된 도구는 생략합니다.
SDK는 [1.0.1 공식 배포](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v1.0.1)의 macOS aarch64 ARM 툴체인을 사용합니다.

```bash
brew install cmake ninja openocd llvm
rustup toolchain install 1.99.0 --profile minimal
rustup target add thumbv7em-none-eabihf --toolchain 1.99.0
```

Rust 버전을 맞추려면 빌드할 터미널에서 적용합니다.

```bash
export RUSTUP_TOOLCHAIN=1.99.0
```

커널과 CMSIS 소스 설치는 [의존성 복원](dependencies.md)을 따릅니다.

## VS Code

프로젝트 루트를 열고 clangd, Rust Analyzer, Microsoft C/C++(GDB), CMake Tools를 설치합니다.
`.vscode/extensions.json`에서 목록을 확인할 수 있습니다.

Tasks의 Configure/Build/Rebuild로 빌드합니다.
C 탐색은 `C/01_led_blink/build/nucleo_f446re/compile_commands.json`을 사용합니다.
Rust 브리지 C 코드를 탐색하려면 settings의 clangd `--compile-commands-dir`을
`Rust/02_led_blink/build/nucleo_f446re`로 바꾸고 clangd를 재시작합니다.
Rust Analyzer에는 두 앱과 `thumbv7em-none-eabihf` 타깃이 등록되어 있습니다.

디버깅은 [OpenOCD 서버](flash-and-debug.md)를 먼저 실행한 뒤 launch 설정을 선택합니다.
