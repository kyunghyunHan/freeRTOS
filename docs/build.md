# 빌드

별도 안내가 없으면 프로젝트 루트 기준입니다. C는 CMake + Ninja, Rust 01·02는 Cargo로 빌드합니다.
기존 C 예제별 폴더는 정리했으므로 아래 경로로 새로 구성하세요.

## C

`register/C/`의 `.c` 파일명에서 확장자를 뺀 이름을 `APP`에 전달합니다.
기본값은 `02_gpio_button`입니다. 파일마다 CMakeLists.txt는 만들지 않습니다.

```bash
cmake -S register/C -B register/C/build/02_gpio_button -G Ninja \
  -DAPP=02_gpio_button -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/build/02_gpio_button

cmake -S register/C -B register/C/build/01_led_blink -G Ninja \
  -DAPP=01_led_blink -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/build/01_led_blink
```

새 파일 `register/C/main.c`를 복사한 경우:

```bash
cmake -S register/C -B register/C/build/main -G Ninja \
  -DAPP=main -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/build/main
```

다른 파일을 선택할 때 `APP`를 바꾸고 별도 빌드 폴더를 사용하세요.
코드 수정 후에는 `cmake --build`만 실행하면 됩니다.
VS Code의 기존 C Configure/Build/Rebuild 태스크도 새 경로에 맞춰져 있습니다.

## Rust — NUCLEO-F429ZI의 01·02 예제

각 예제 폴더에서 Cargo만으로 최종 ELF를 생성합니다. 세 예제의 출력 파일명은 모두 `firmware`입니다.

```bash
cd register/Rust/01_hello_world
cargo build --release --target thumbv7em-none-eabihf
openocd -f board/st_nucleo_f4.cfg \
  -c "program target/thumbv7em-none-eabihf/release/firmware verify reset exit"
```

02 LED는 프로젝트 루트에서 다음처럼 실행합니다.

```bash
cd register/Rust/02_led_blink
cargo build --release --target thumbv7em-none-eabihf
openocd -f board/st_nucleo_f4.cfg \
  -c "program target/thumbv7em-none-eabihf/release/firmware verify reset exit"
```

`src/lib.rs`라는 파일명은 유지하지만 Cargo의 실행 파일 타깃입니다. `.a`가 아니라 확장자 없는 ELF를 생성합니다.
`build.rs`와 공유 `../support/cargo_build.rs`가 FreeRTOS·CMSIS를 ARM GCC로 빌드하고 Rust linker가 최종 연결합니다.
`01_hello_world`의 현재 사용자 코드는 F429ZI LED·버튼 제어이며, 문자열 Hello World는 `03_hello_world_cargo`에 있습니다.
`02_led_blink`는 F429ZI LD1(PB0)을 매초 토글하고 USART3(PD8/PD9)에 시작 로그를 출력합니다.
기본 ARM GCC 경로는 `~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc`이고
다른 위치는 `ARM_GCC=/absolute/path/to/gcc cargo build ...`로 지정합니다.
보드 기록과 실제 동작은 미검증입니다.

기존 CMake 사용도 가능하며, 내부에서 Cargo release 빌드를 호출합니다(01 또는 02 경로 선택).

```bash
cmake -S register/Rust/02_led_blink -B register/Rust/02_led_blink/build/nucleo_f429zi -G Ninja
cmake --build register/Rust/02_led_blink/build/nucleo_f429zi
```

이 경우 해당 build 폴더에 `firmware.elf`, `.hex`, `.bin`을 생성합니다.
F446RE용 기존 빌드 폴더는 사용하지 마세요. `RUST_PROVIDES_MAIN` 옵션은 필요하지 않습니다.

## Rust — NUCLEO-F429ZI 버튼 예제

프로젝트 루트에서 시작합니다. B1(PC13, 누르면 HIGH)을 누를 때 LD1(PB0)을 한 번 토글합니다.
10ms 폴링과 30ms 디바운스를 사용하며, UART 출력은 없습니다.

```bash
cd register/Rust/02_gpio_button
cargo build --release --target thumbv7em-none-eabihf
openocd -f board/st_nucleo_f4.cfg \
  -c "program target/thumbv7em-none-eabihf/release/firmware verify reset exit"
```

이 폴더도 CMake 없이 Cargo로 최종 ELF를 생성합니다. CMake 호환 빌드는
`-B register/Rust/02_gpio_button/build/nucleo_f429zi`를 사용합니다.

## clean / 재구성

```bash
cmake --build register/C/build/02_gpio_button --target clean
cmake --build register/C/build/02_gpio_button --clean-first
```

Cargo 예제는 해당 폴더에서 `cargo clean` 후 다시 빌드합니다. CMake 호환 빌드의
`--clean-first`는 Cargo 캐시를 지우지 않으며 Cargo가 변경된 소스만 재컴파일합니다.

캐시를 초기화하려면 최초 구성 명령에 `--fresh`를 추가하고 모든 옵션을 다시 전달합니다.

## 결과 파일

| 파일 | 용도 |
|---|---|
| firmware.elf | Flash/GDB 입력, 디버그 심볼 |
| firmware.hex | 주소가 포함된 Intel HEX |
| firmware.bin | raw 이미지, Flash 주소 0x08000000 별도 지정 |
| firmware.map | 코드·데이터·heap·스택 배치 |
| compile_commands.json | C/ASM 탐색용 컴파일 명령 |
| target/thumbv7em-none-eabihf/release/firmware | Rust 01·02 예제의 Cargo ELF |

호환 설정과 공통 지원 코드는 [CubeIDE 문서](cubeide.md)를 참고하세요.
