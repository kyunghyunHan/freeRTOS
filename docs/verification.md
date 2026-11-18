# 검증 결과

2026-11-03 / macOS Apple Silicon / NUCLEO-F446RE

| 예제 | ELF/HEX/BIN | Flash 크기 | 정적 RAM | MSP 예약 |
|---|---|---:|---:|---:|
| register/C/01_led_blink | 빌드 성공 | 7488 B | 33096 B | 4096 B |
| register/Rust/01_hello_world | 빌드 성공 | 8856 B | 33096 B | 4096 B |
| register/Rust/02_led_blink | 빌드 성공 | 8932 B | 33096 B | 4096 B |

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

## 2026-11-17 GPIO 버튼 추가

| 예제 | ELF/HEX/BIN | Flash 크기 | 정적 RAM | MSP 예약 |
|---|---|---:|---:|---:|
| register/C/02_gpio_button | 빌드 성공 | 7624 B | 33096 B | 4096 B |
| register/Rust/02_gpio_button | 빌드 성공 | 8912 B | 33096 B | 4096 B |

Rust 포맷 검사와 기존 세 예제의 재빌드도 확인했습니다.
실제 보드 기록, B1 입력 극성·디바운스·길게 누름·해제 후 재누름 동작은 보드에서 미검증입니다.

## 2026-11-17 register / hal 폴더 분리

기존 C/Rust 예제를 `register/`로 이동했습니다. `hal/`은 구현 예정입니다.
새 경로에서 다섯 예제를 `cmake --fresh`로 구성하고 ELF/HEX/BIN 빌드를 확인했습니다.
VS Code JSON 구문과 문서의 로컬 링크도 확인했습니다. 보드 기록은 실행하지 않았습니다.

## 2026-11-17 예제 소스에서 직접 레지스터 접근

보드 추상화와 Rust 보드 API를 제거했습니다. C는 각 `src/main.c`에서 CMSIS 레지스터를,
Rust는 각 `src/lib.rs`에서 실제 주소에 volatile 읽기/쓰기를 사용합니다.
FreeRTOS 설정·링커·오류 hook은 예제별로 포함하고, 컴파일·링크 설정만 `cmake/`에 둡니다.

| 예제 | ELF/HEX/BIN | Flash 크기 | 정적 RAM | MSP 예약 |
|---|---|---:|---:|---:|
| register/C/01_led_blink | 빌드 성공 | 7388 B | 33096 B | 4096 B |
| register/C/02_gpio_button | 빌드 성공 | 7524 B | 33096 B | 4096 B |
| register/Rust/01_hello_world | 빌드 성공 | 7564 B | 33096 B | 4096 B |
| register/Rust/02_led_blink | 빌드 성공 | 7612 B | 33096 B | 4096 B |
| register/Rust/02_gpio_button | 빌드 성공 | 7596 B | 33096 B | 4096 B |

다섯 예제를 새로 구성한 뒤 빌드했고, Rust 포맷을 정리했습니다.
최종 ELF의 커널 인터럽트 핸들러 및 기존 보드 함수 심볼 제거,
VS Code JSON과 로컬 문서 링크를 확인했습니다. 보드 동작은 미검증입니다.

## 2026-11-18 단일 C 파일과 CubeIDE 호환 지원

C 소스는 `register/C/01_led_blink.c`, `02_gpio_button.c`로 이동했습니다.
입력 시점의 두 C main.c 내용(사용자가 수정한 01 포함)과 Rust main.c 세 개는 SHA256 비교로
변경하지 않았음을 확인했습니다. FreeRTOS 커널의 모든 C/H 소스도 변경하지 않았습니다.
기존 C 폴더의 빌드 캐시는 `.cache/legacy-c-builds/`로 옮겼습니다.

공통 weak Hook·assert·fault 진단과 scheduler 전 SystemCoreClock 갱신을 추가했습니다.
SDK picolibc의 `__bothinit_array_start/end`를 공통 및 Rust 링커에 연결했습니다.
현재 툴체인에는 _init/_fini 기본 구현이 필요하지 않음을 무정의 fixture 링크로 확인했습니다.

| 예제 | clean build / ELF·HEX·BIN | Flash | 정적 RAM | MSP |
|---|---|---:|---:|---:|
| C/01_led_blink.c | 성공 | 7420 B | 33096 B | 4096 B |
| C/02_gpio_button.c | 성공 | 7660 B | 33096 B | 4096 B |
| Rust/01_hello_world | 성공 | 7700 B | 33096 B | 4096 B |
| Rust/02_led_blink | 성공 | 7748 B | 33096 B | 4096 B |
| Rust/02_gpio_button | 성공 | 7732 B | 33096 B | 4096 B |

각 프로젝트를 --fresh 구성 후 --clean-first 빌드했습니다. Rust Cargo는 --locked --offline으로
최신 상태를 확인했습니다. Rust 소스는 변경하지 않아 Cargo 전체 캐시 삭제는 수행하지 않았습니다.
최종 ELF의 미해결 심볼 없음, SVC/PendSV/SysTick 및 HardFault 벡터 연결,
초기화 배열의 런타임 진입점, 앱 strong 함수 우선 / 기본 weak 함수 선택을 확인했습니다.

`python3 tests/verify_c_link.py`의 세 경우도 모두 통과했습니다.
Hook 없는 단일 C, malloc Hook만 재정의한 C, malloc·스택 Hook 비활성 설정을 실제 ARM 타깃으로
컴파일·링크하고, 초기화 배열·클록 갱신 순서·hard-float·결과 파일을 검사합니다.

실행한 명령의 원문과 빌드 로그는 `.cache/verification/2026-11-18/*.log`에 있습니다.
호환성 fixture 로그는 `.cache/verification/no_hooks.log`, `partial_override.log`,
`hooks_disabled.log`입니다. 입력 파일 해시는 `.cache/verification/cubeide-input-sha256.json`에 있습니다.

ST-LINK USB와 시리얼 포트는 보이지만, CubeIDE의 ST-LINK_gdbserver가 장치를 독점하고
61234/61235 포트를 사용 중이었습니다. 기존 디버깅 세션을 종료하거나 Flash를 덮어쓰지 않았습니다.
따라서 이번 결과는 빌드·정적 링크 검증이며, 보드 Flash·실제 tick·버튼·LED 동작 검증은 아닙니다.

현재 명령과 설정·파일 역할·호환 제한은 [빌드](build.md), [CubeIDE 호환](cubeide.md)에 정리했습니다.
