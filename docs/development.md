# 새 예제 추가

C 예제는 `register/C/`에 `.c` 파일 하나만 추가합니다.

```bash
cp /path/to/CubeIDE/Core/Src/main.c register/C/03_uart.c
cmake -S register/C -B register/C/build/03_uart -G Ninja \
  -DAPP=03_uart -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/build/03_uart
```

`APP`는 확장자를 뺀 파일명입니다. CMakeLists.txt 수정이나 새 소스 폴더는 필요 없습니다.
C 공통 FreeRTOSConfig.h와 링커는 `cmake/`에 있습니다. Hook은 자동 연결됩니다.
프로젝트별 설정과 추가 헤더는 [CubeIDE 호환 문서](cubeide.md)의 CMake 옵션으로 지정합니다.
03~07의 기존 예약 설명은 `docs/c-examples/`에 보관합니다.

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
