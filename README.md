# FreeRTOS C / Rust 실습 — NUCLEO-F446RE

macOS Apple Silicon / STM32 NUCLEO-F446RE / 내장 ST-LINK.
C는 CMake와 Ninja, F429ZI용 Rust 예제는 Cargo로 최종 펌웨어를 빌드합니다.

```text
freeRTOS/
├── register/              # 레지스터 직접 제어 버전
│   ├── C/                # .c 예제 파일 + CMakeLists.txt 하나
│   └── Rust/             # UART 로그, LED, 버튼 예제
├── hal/                   # HAL 버전용 예약 폴더 (아직 미구현)
├── cmake/                 # C 공통 설정/링커/FreeRTOS 지원 코드
├── external/              # 공식 의존성, dependencies.lock.json
├── docs/
└── .vscode/
```

C 실습은 `register/C/01_led_blink.c`, `02_gpio_button.c`처럼 파일 하나로 작성합니다.
새 `.c` 파일을 추가해도 CMakeLists.txt를 수정할 필요가 없습니다.
FreeRTOS Hook은 자동 제공되며 GPIO는 예제에서 레지스터를 직접 다룹니다.
Rust 예제는 Rust `main()`에서 시작하며 태스크 생성과 GPIO/UART 제어도 Rust로 작성합니다.
FreeRTOS 커널과 공통 CMSIS 런타임은 C/ASM으로 연결합니다.
[HAL 예약 폴더](hal/README.md)는 유지합니다.

NUCLEO-F429ZI의 01·02 빌드와 기록 명령은 [빌드 문서](docs/build.md)에 있습니다.
문자열 Hello World 예제는
[Cargo Hello World 예제](register/Rust/03_hello_world_cargo/README.md)를 참고하세요.

## C 빌드

프로젝트 루트에서 실행합니다.

```bash
cmake -S register/C -B register/C/build/01_led_blink -G Ninja \
  -DAPP=01_led_blink -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/build/01_led_blink
```
```
openocd -f board/st_nucleo_f4.cfg \
  -c "program register/C/build/01_led_blink/firmware.elf verify reset exit"
```

`APP`는 확장자를 뺀 파일명입니다. CubeIDE의 `main.c`를 `register/C/main.c`로 복사했다면
`-DAPP=main -B register/C/build/main`을 사용합니다. 앱마다 빌드 폴더를 나눕니다.
결과는 해당 빌드 폴더의 `firmware.elf`, `firmware.hex`, `firmware.bin`입니다.

자세한 [빌드 방법](docs/build.md), [CubeIDE 호환 범위](docs/cubeide.md),
[C 버튼 설명](docs/c-examples/02_gpio_button.md), [Rust 버튼 설명](register/Rust/02_gpio_button/README.md)을 참고하세요.

다섯 예제의 ELF/HEX/BIN 빌드는 확인했습니다. 보드 기록과 LED/UART 동작은 미검증입니다.

| 문서 | 내용 |
|---|---|
| [환경 설정](docs/setup.md) | 도구 경로, 설치, VS Code |
| [의존성](docs/dependencies.md) | 고정 버전과 소스 복원 |
| [빌드](docs/build.md) | 최초 설정, 재빌드, clean, 결과 파일 |
| [Flash·디버그](docs/flash-and-debug.md) | 기록, 전체 삭제, UART, GDB |
| [RTOS 원리](docs/rtos.md) | 태스크, 메모리, 인터럽트, Zephyr 비교 |
| [Rust](docs/rust.md) | C ABI, unsafe, panic, 메모리 |
| [CubeIDE 호환](docs/cubeide.md) | main.c 복사, 설정, 제한사항 |
| [예제 추가](docs/development.md) | C/Rust 앱 추가 |
| [문제 해결](docs/troubleshooting.md) | 빌드·연결 오류 |
| [검증 결과](docs/verification.md) | 빌드 결과와 미검증 항목 |


- [ARM 프로그래밍 실무 — FreeRTOS](https://wikidocs.net/160931)
  - RTOS 기본 개념
  - FreeRTOS Task
  - CMSIS-RTOS
  - STM32에서의 RTOS 활용

- [리눅스로 공부하는 운영체제](https://wikidocs.net/book/9819)
  - 프로세스와 스레드
  - CPU 스케줄링
  - 프로세스 동기화, Race Condition, Critical Section
  - 메모리 관리, 인터럽트, DMA, 디바이스 드라이버
  - RTOS 학습 전 운영체제 기초 추천
