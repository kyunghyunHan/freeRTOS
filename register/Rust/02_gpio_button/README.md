# 02 GPIO 버튼 — Rust

NUCLEO-F429ZI 내장 B1 USER 버튼 하나로 LD1 LED를 토글합니다. 추가 배선은 필요 없습니다.

- B1 USER(파란색): PC13, 누르면 HIGH, 놓으면 LOW.
- LD1(녹색): PB0, 시작 시 꺼짐.
- FreeRTOS 애플리케이션 태스크 하나가 10ms 간격으로 입력을 읽습니다.
- 같은 입력이 30ms 동안 유지되면 눌림/해제를 확정합니다.
- 눌림이 확정될 때만 LED를 한 번 토글합니다. 길게 누르거나 놓을 때는 토글하지 않습니다.
- 리셋할 때 누르고 있으면 디바운스 후 한 번 토글합니다. 매우 짧은 누름은 무시될 수 있습니다.

보드 핀·극성 기준: [Zephyr 공식 F429ZI 보드 정의](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/nucleo_f429zi/nucleo_f429zi.dts).
Rust `src/lib.rs`가 main과 태스크를 정의하고 실제 레지스터 주소에 volatile 읽기/쓰기를 수행합니다.
태스크 생성과 스케줄러 시작도 Rust의 공유 `../support/startup.rs`에서 처리합니다. 앱의 C 파일은 없습니다.

## 빌드

이 예제 폴더에서 실행합니다.

```bash
cargo build --release --target thumbv7em-none-eabihf
openocd -f board/st_nucleo_f4.cfg \
  -c "program target/thumbv7em-none-eabihf/release/firmware verify reset exit"
```

출력 `target/thumbv7em-none-eabihf/release/firmware`는 확장자가 없는 ARM ELF입니다.
`build.rs`가 공유 `../support/cargo_build.rs`를 통해 FreeRTOS·CMSIS를 빌드하고 연결합니다.
앱의 진입점과 태스크, GPIO 제어는 Rust입니다. CMake 없이 빌드할 수 있습니다.
VS Code Build/Rebuild/Cargo check도 Cargo를 사용합니다. UART 문자열 출력은 없습니다.

기존 CMake 방식은 F429ZI 폴더로 새로 구성해 사용합니다(프로젝트 루트 기준).

```bash
cmake -S register/Rust/02_gpio_button -B register/Rust/02_gpio_button/build/nucleo_f429zi -G Ninja
cmake --build register/Rust/02_gpio_button/build/nucleo_f429zi
```

이 방식은 해당 build 폴더에 `firmware.elf`, `.hex`, `.bin`을 만듭니다.
F446RE용 기존 build 폴더는 사용하지 마세요.

## 보드 확인

1. 펌웨어를 기록하고 리셋하면 LED가 꺼져 있어야 합니다.
2. B1을 누르면 LED가 켜지고, 길게 누른 상태에서는 계속 켜져 있어야 합니다.
3. B1을 놓아도 LED 상태가 유지되어야 합니다.
4. 다시 누르면 LED가 꺼져야 합니다.

ELF/HEX/BIN 빌드는 확인했습니다. 실제 보드 기록과 버튼/LED 동작은 미검증입니다.
