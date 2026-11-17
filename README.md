# FreeRTOS C / Rust 실습 — NUCLEO-F446RE

macOS Apple Silicon / STM32 NUCLEO-F446RE / 내장 ST-LINK.
CMake와 Ninja로 빌드하며, Rust 예제는 Cargo로 만든 정적 라이브러리를 FreeRTOS와 연결합니다.

```text
freeRTOS/
├── register/              # 레지스터 직접 제어 버전
│   ├── C/                # LED, 버튼 예제 + 03~07 예약 폴더
│   └── Rust/             # UART 로그, LED, 버튼 예제
├── hal/                   # HAL 버전용 예약 폴더 (아직 미구현)
├── cmake/                 # 컴파일·링크 설정만 공유
├── external/              # 공식 의존성, dependencies.lock.json
├── docs/
└── .vscode/
```

현재 구현은 모두 `register/`에 있습니다. C는 CMSIS 레지스터 정의로 직접 접근하고,
Rust는 실제 레지스터 주소에 `read_volatile` / `write_volatile`을 사용합니다.
각 예제는 `FreeRTOSConfig.h`, 링커 스크립트, 오류 hook을 자체 포함합니다.
`hal/`의 구분과 구현 상태는 [HAL 안내](hal/README.md)를 참고하세요.

소스는 각 앱의 `src/`, 빌드 결과는 `build/nucleo_f446re/`에 있습니다.
C 03~07은 구현 전 폴더입니다.
버튼 예제: [C 02](register/C/02_gpio_button/README.md), [Rust 02](register/Rust/02_gpio_button/README.md).

## 빌드

최초 설정은 [빌드 문서](docs/build.md)를 따릅니다. 이후 코드 수정 시:

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
cmake --build register/C/01_led_blink/build/nucleo_f446re
cmake --build register/C/02_gpio_button/build/nucleo_f446re
cmake --build register/Rust/01_hello_world/build/nucleo_f446re
cmake --build register/Rust/02_led_blink/build/nucleo_f446re
cmake --build register/Rust/02_gpio_button/build/nucleo_f446re
```

다섯 예제의 ELF/HEX/BIN 빌드는 확인했습니다. 보드 기록과 LED/UART 동작은 미검증입니다.

| 문서 | 내용 |
|---|---|
| [환경 설정](docs/setup.md) | 도구 경로, 설치, VS Code |
| [의존성](docs/dependencies.md) | 고정 버전과 소스 복원 |
| [빌드](docs/build.md) | 최초 설정, 재빌드, clean, 결과 파일 |
| [Flash·디버그](docs/flash-and-debug.md) | 기록, 전체 삭제, UART, GDB |
| [RTOS 원리](docs/rtos.md) | 태스크, 메모리, 인터럽트, Zephyr 비교 |
| [Rust](docs/rust.md) | C ABI, unsafe, panic, 메모리 |
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