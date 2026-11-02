# 검증 결과

2026-11-03 / macOS Apple Silicon / NUCLEO-F446RE

| 예제 | ELF/HEX/BIN | Flash 크기 | 정적 RAM | MSP 예약 |
|---|---|---:|---:|---:|
| C/01_led_blink | 빌드 성공 | 7488 B | 33096 B | 4096 B |
| Rust/01_hello_world | 빌드 성공 | 8856 B | 33096 B | 4096 B |
| Rust/02_led_blink | 빌드 성공 | 8932 B | 33096 B | 4096 B |

정적 RAM에는 heap_4의 32KiB가 포함됩니다.

확인한 항목:

- 세 예제의 최초·전체·증분 빌드, Rust Cargo clean/check/fmt
- Rust 앱과 공통 API 변경 후 재컴파일·재링크
- ELF 벡터, 인터럽트 핸들러, heap/MSP 크기, hard-float ABI
- GDB의 오프라인 ELF·심볼 읽기
- VS Code JSON, 문서 링크, 명령 구문

로그와 ELF SHA256은 `.cache/verification/`에 있습니다.

보드 기록·전체 삭제는 실행하지 않았습니다. LED/UART 동작, 실제 타이밍·스택 사용량,
오류 hook 실행, 보드 GDB 연결 및 VS Code 확장 동작은 미검증입니다.
