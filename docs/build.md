# 빌드

모든 명령은 프로젝트 루트 기준입니다. CMake + Ninja와 `arm-zephyr-eabi-gcc`를 사용합니다.
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

## Rust

```bash
cmake -S register/Rust/01_hello_world -B register/Rust/01_hello_world/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/Rust/01_hello_world/build/nucleo_f446re
cmake -S register/Rust/02_led_blink -B register/Rust/02_led_blink/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/Rust/02_led_blink/build/nucleo_f446re
cmake -S register/Rust/02_gpio_button -B register/Rust/02_gpio_button/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/Rust/02_gpio_button/build/nucleo_f446re
```

Rust는 의존성 crate 없이 `core`만 사용합니다. Cargo는 `--locked --offline`으로 실행됩니다.
CMake가 Cargo 정적 라이브러리를 커널과 최종 링크합니다.
CMake Debug, Rust dev profile(opt-level=s, panic=abort)를 사용합니다.
CMake `Release`는 Cargo의 `--release`를 자동 선택하지 않습니다.

## clean / 재구성

```bash
cmake --build register/C/build/02_gpio_button --target clean
cmake --build register/C/build/02_gpio_button --clean-first
```

Rust의 `--clean-first`는 Cargo 캐시를 지우지 않습니다. 전체 재컴파일이 필요하면:

```bash
cargo clean --manifest-path register/Rust/01_hello_world/Cargo.toml \
  --target-dir register/Rust/01_hello_world/build/nucleo_f446re/rust/target
cmake --build register/Rust/01_hello_world/build/nucleo_f446re --clean-first
```

캐시를 초기화하려면 최초 구성 명령에 `--fresh`를 추가하고 모든 옵션을 다시 전달합니다.

## 결과 파일

| 파일 | 용도 |
|---|---|
| firmware.elf | Flash/GDB 입력, 디버그 심볼 |
| firmware.hex | 주소가 포함된 Intel HEX |
| firmware.bin | raw 이미지, Flash 주소 0x08000000 별도 지정 |
| firmware.map | 코드·데이터·heap·스택 배치 |
| compile_commands.json | C/ASM 탐색용 컴파일 명령 |
| rust/target/thumbv7em-none-eabihf/debug/librustapp.a | Rust 앱의 정적 라이브러리 |

호환 설정과 공통 지원 코드는 [CubeIDE 문서](cubeide.md)를 참고하세요.
