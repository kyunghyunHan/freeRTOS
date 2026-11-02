# 빌드와 clean

모든 명령은 프로젝트 루트에서 실행합니다.

## 최초 설정

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
cmake -S C/01_led_blink -B C/01_led_blink/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/platform/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build C/01_led_blink/build/nucleo_f446re
cmake -S Rust/01_hello_world -B Rust/01_hello_world/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/platform/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build Rust/01_hello_world/build/nucleo_f446re
cmake -S Rust/02_led_blink -B Rust/02_led_blink/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/platform/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build Rust/02_led_blink/build/nucleo_f446re
```

## 코드 수정 후 빌드

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
cmake --build C/01_led_blink/build/nucleo_f446re
cmake --build Rust/01_hello_world/build/nucleo_f446re
cmake --build Rust/02_led_blink/build/nucleo_f446re
```

Ninja를 직접 실행해도 같습니다.

```bash
ninja -C C/01_led_blink/build/nucleo_f446re
ninja -C Rust/01_hello_world/build/nucleo_f446re
ninja -C Rust/02_led_blink/build/nucleo_f446re
```

## 전체 재빌드

C:

```bash
cmake --build C/01_led_blink/build/nucleo_f446re --clean-first
```

Rust는 Cargo 캐시도 비웁니다.

```bash
cargo clean --manifest-path Rust/01_hello_world/Cargo.toml --target-dir Rust/01_hello_world/build/nucleo_f446re/rust/target
cmake --build Rust/01_hello_world/build/nucleo_f446re --clean-first
cargo clean --manifest-path Rust/02_led_blink/Cargo.toml --target-dir Rust/02_led_blink/build/nucleo_f446re/rust/target
cmake --build Rust/02_led_blink/build/nucleo_f446re --clean-first
```

## clean

소스와 CMake 캐시를 유지하고 빌드 결과만 지웁니다.

```bash
cmake --build C/01_led_blink/build/nucleo_f446re --target clean
cmake --build Rust/01_hello_world/build/nucleo_f446re --target clean
cargo clean --manifest-path Rust/01_hello_world/Cargo.toml --target-dir Rust/01_hello_world/build/nucleo_f446re/rust/target
cmake --build Rust/02_led_blink/build/nucleo_f446re --target clean
cargo clean --manifest-path Rust/02_led_blink/Cargo.toml --target-dir Rust/02_led_blink/build/nucleo_f446re/rust/target
```

구성부터 다시 시작할 때는 대상 앱의 빌드 폴더만 삭제합니다.
C 빌드 폴더를 삭제한 뒤 위의 최초 설정 명령을 다시 실행합니다.

```bash
rm -rf C/01_led_blink/build/nucleo_f446re
```

`--clean-first`는 Cargo 캐시를 지우지 않으므로 Rust 전체 재빌드에는 `cargo clean`도 필요합니다.
CMake 캐시를 바꿀 때는 `cmake --fresh -S ... -B ...`에 최초의 toolchain/Debug 옵션을 모두 다시 전달하세요.

## Cargo 직접 실행

두 앱은 로컬 path 의존성 하나뿐이고 crates.io에 접근하지 않습니다.
Cargo만 실행하면 정적 라이브러리까지 생성하며 보드용 최종 ELF는 CMake 링크가 필요합니다.

```bash
cargo build --manifest-path Rust/01_hello_world/Cargo.toml --target thumbv7em-none-eabihf \
  --target-dir Rust/01_hello_world/build/nucleo_f446re/rust/target --locked --offline
cargo build --manifest-path Rust/02_led_blink/Cargo.toml --target thumbv7em-none-eabihf \
  --target-dir Rust/02_led_blink/build/nucleo_f446re/rust/target --locked --offline
cmake --build Rust/01_hello_world/build/nucleo_f446re
cmake --build Rust/02_led_blink/build/nucleo_f446re
```

CMake Debug, Rust dev profile(opt-level=s, debug info, panic=abort)를 사용합니다.
CMake `Release`로 바꿔도 Cargo는 자동으로 `--release`로 바뀌지 않습니다.

## 결과 파일

세 앱 모두 `앱/build/nucleo_f446re/`에 다음 파일을 생성합니다.

| 파일 | 용도 |
|---|---|
| firmware.elf | 디버그 심볼과 load 주소가 있는 Flash/GDB 입력 |
| firmware.hex | 주소가 포함된 Intel HEX |
| firmware.bin | raw 이미지, Flash 주소 0x08000000을 따로 지정 |
| firmware.map | 함수·데이터·heap·스택의 링크 배치 |
| compile_commands.json | C/ASM 코드 탐색용 실제 컴파일 명령 |
| rust/target/thumbv7em-none-eabihf/debug/librustapp.a | Rust 앱만의 정적 라이브러리, Rust 앱에만 있음 |

