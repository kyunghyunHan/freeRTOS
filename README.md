# FreeRTOS C / Rust 실습 — NUCLEO-F446RE

macOS Apple Silicon / STM32 NUCLEO-F446RE / 내장 ST-LINK.
CMake와 Ninja로 빌드하며, Rust 예제는 Cargo로 만든 정적 라이브러리를 FreeRTOS와 연결합니다.

```text
freeRTOS/
├── C/01_led_blink/          # FreeRTOS 태스크: LD2 PA5, 매 1000ms 토글
├── C/02_gpio_button/       # 이하 07_queue까지 README 예약 폴더
├── Rust/01_hello_world/    # FreeRTOS 태스크에서 USART2 로그
├── Rust/02_led_blink/      # 같은 커널 위 Rust 태스크: LED + 시작 로그
├── platform/              # 공통 보드/설정/링커/C ABI/Rust API
├── external/              # 고정 공식 소스, dependencies.lock.json
├── docs/
└── .vscode/
```

소스는 각 앱의 `src/`, 빌드 결과는 `build/nucleo_f446re/`에 있습니다.
C 02~07은 구현 전 폴더입니다.

## 빌드

최초 설정은 [빌드 문서](docs/build.md)를 따릅니다. 이후 코드 수정 시:

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
cmake --build C/01_led_blink/build/nucleo_f446re
cmake --build Rust/01_hello_world/build/nucleo_f446re
cmake --build Rust/02_led_blink/build/nucleo_f446re
```

세 예제의 ELF/HEX/BIN 빌드는 확인했습니다. 보드 기록과 LED/UART 동작은 미검증입니다.

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
