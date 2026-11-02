# 새 예제 추가

모든 명령은 프로젝트 루트 기준입니다. 기존 대상 폴더가 있으면 덮어쓰지 마세요.

## C

```bash
mkdir -p C/08_my_task/src
cp -n C/01_led_blink/CMakeLists.txt C/08_my_task/CMakeLists.txt
cp -n C/01_led_blink/src/main.c C/08_my_task/src/main.c
```

CMake의 project 이름과 태스크 소스를 수정합니다. `include(../../platform/firmware.cmake)`는 유지하고
새 C 파일은 `target_sources(firmware PRIVATE src/추가파일.c)`에 등록합니다.
새 앱도 main에서 board_init 후 태스크 생성 결과를 검사하고 scheduler를 시작해야 합니다.

```bash
cmake -S C/08_my_task -B C/08_my_task/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/platform/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build C/08_my_task/build/nucleo_f446re
```

02~07 예약 폴더도 같은 최소 파일을 추가하면 됩니다. 현재 README만 있고 빌드 대상은 아닙니다.

## Rust

빌드 산출물을 복사하지 않고 필요한 소스만 가져옵니다.

```bash
mkdir -p Rust/03_my_task/src
cp -n Rust/01_hello_world/CMakeLists.txt Rust/03_my_task/CMakeLists.txt
cp -n Rust/01_hello_world/Cargo.toml Rust/03_my_task/Cargo.toml
cp -n Rust/01_hello_world/Cargo.lock Rust/03_my_task/Cargo.lock
cp -n Rust/01_hello_world/src/lib.rs Rust/03_my_task/src/lib.rs
```

CMake project 이름을 수정하고 staticlib 이름 rustapp은 유지합니다.
메시지/앱 로직을 수정한 뒤 빌드합니다.

```bash
cmake -S Rust/03_my_task -B Rust/03_my_task/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/platform/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build Rust/03_my_task/build/nucleo_f446re
```

새 API는 rust_bridge.h/.c와 rust-api/src/lib.rs 양쪽의 ABI 및 안전 계약을 함께 추가합니다.
다중 태스크는 BoardTask 독점 소유 설계를 먼저 바꿔야 합니다.
Rust Analyzer linkedProjects 및 필요한 tasks/launch의 앱 경로도 추가하세요.
공통 FreeRTOSConfig/board 변경은 세 기존 앱에도 영향을 주므로 모두 다시 빌드하세요.
