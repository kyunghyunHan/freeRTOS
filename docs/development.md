# 새 예제 추가

모든 명령은 프로젝트 루트 기준입니다. 기존 대상 폴더가 있으면 덮어쓰지 마세요.
각 예제는 레지스터 초기화, FreeRTOS 설정, 링커, hook을 자체 포함합니다.

## C

빌드 산출물을 제외하고 필요한 소스와 설정만 복사합니다.

```bash
mkdir -p register/C/08_my_task/src
cp -n register/C/01_led_blink/CMakeLists.txt register/C/08_my_task/
cp -n register/C/01_led_blink/FreeRTOSConfig.h register/C/08_my_task/
cp -n register/C/01_led_blink/stm32f446re.ld register/C/08_my_task/
cp -n register/C/01_led_blink/src/main.c register/C/08_my_task/src/
```

CMake project 이름과 `src/main.c`를 수정합니다.
`include(../../../cmake/firmware.cmake)`는 컴파일·링크 설정만 공유합니다.
GPIO/UART는 예제 소스에서 직접 레지스터를 다룹니다.

```bash
cmake -S register/C/08_my_task -B register/C/08_my_task/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/08_my_task/build/nucleo_f446re
```

03~07 예약 폴더도 같은 최소 파일을 추가하면 됩니다. 현재 README만 있고 빌드 대상은 아닙니다.

## Rust

```bash
mkdir -p register/Rust/03_my_task/src
cp -n register/Rust/01_hello_world/CMakeLists.txt register/Rust/03_my_task/
cp -n register/Rust/01_hello_world/Cargo.toml register/Rust/03_my_task/
cp -n register/Rust/01_hello_world/Cargo.lock register/Rust/03_my_task/
cp -n register/Rust/01_hello_world/FreeRTOSConfig.h register/Rust/03_my_task/
cp -n register/Rust/01_hello_world/stm32f446re.ld register/Rust/03_my_task/
cp -n register/Rust/01_hello_world/src/main.c register/Rust/03_my_task/src/
cp -n register/Rust/01_hello_world/src/lib.rs register/Rust/03_my_task/src/
```

CMake project 이름을 수정하고 staticlib 이름 rustapp은 유지합니다.
`src/main.c`는 커널 시작을 담당하고, `src/lib.rs`가 주변장치 레지스터를 직접 다룹니다.
의존성 crate는 없습니다. tick 설정을 바꾸면 Rust의 `vTaskDelay` ABI와 지연값도 맞춰야 합니다.

```bash
cmake -S register/Rust/03_my_task -B register/Rust/03_my_task/build/nucleo_f446re -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/Rust/03_my_task/build/nucleo_f446re
```

여러 태스크가 같은 주변장치를 사용하면 레지스터 접근 경합을 직접 처리해야 합니다.
Rust Analyzer linkedProjects 및 필요한 tasks/launch의 앱 경로도 추가하세요.
공유 CMake 변경은 기존 다섯 앱을 모두 빌드해 확인합니다.
