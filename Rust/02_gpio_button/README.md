# 02 GPIO 버튼 — Rust

NUCLEO-F446RE 내장 B1 USER 버튼 하나로 LD2 LED를 토글합니다. 추가 배선은 필요 없습니다.

- B1 USER(파란색): PC13, 누르면 LOW, 놓으면 HIGH.
- LD2(녹색): PA5, 시작 시 꺼짐.
- FreeRTOS 애플리케이션 태스크 하나가 10ms 간격으로 입력을 읽습니다.
- 같은 입력이 30ms 동안 유지되면 눌림/해제를 확정합니다.
- 눌림이 확정될 때만 LED를 한 번 토글합니다. 길게 누르거나 놓을 때는 토글하지 않습니다.
- 리셋할 때 누르고 있으면 디바운스 후 한 번 토글합니다. 매우 짧은 누름은 무시될 수 있습니다.

버튼 회로 기준: [ST MB1136 회로도, MCU 페이지](https://www.st.com/resource/en/schematic_pack/mb1136-default-c04_schematic.pdf).
GPIO 레지스터 접근은 공통 `platform/board.c`에 있으며, C/Rust 앱에 같은 디바운스 로직을 구현했습니다.

## 빌드

프로젝트 루트에서 실행합니다.

```bash
cmake -S Rust/02_gpio_button -B Rust/02_gpio_button/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/platform/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build Rust/02_gpio_button/build/nucleo_f446re
```

결과: `Rust/02_gpio_button/build/nucleo_f446re/firmware.elf`, `firmware.hex`, `firmware.bin`.
VS Code의 `Rust/02_gpio_button: Build` 태스크로도 빌드할 수 있습니다.
보드 기록 방법은 [Flash·디버그](../../docs/flash-and-debug.md)를 참고하세요.

## 보드 확인

1. 펌웨어를 기록하고 리셋하면 LED가 꺼져 있어야 합니다.
2. B1을 누르면 LED가 켜지고, 길게 누른 상태에서는 계속 켜져 있어야 합니다.
3. B1을 놓아도 LED 상태가 유지되어야 합니다.
4. 다시 누르면 LED가 꺼져야 합니다.

ELF/HEX/BIN 빌드는 확인했습니다. 실제 보드 기록과 버튼/LED 동작은 미검증입니다.
