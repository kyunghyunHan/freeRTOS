# Rust와 C 연결

C가 보드와 FreeRTOS를 초기화하고, FreeRTOS 태스크에서 `no_std` Rust 함수를 호출합니다.

## 구성

| 파일 | 역할 |
|---|---|
| platform/rust_entry.c | 태스크 생성, 스케줄러 시작, rust_task 호출 |
| platform/rust_bridge.h / .c | delay, LED, UART, panic의 C ABI |
| platform/rust-api/src/lib.rs | Rust API와 panic_handler |
| Rust/*/src/lib.rs | 예제 로직 |

커뮤니티 바인딩인 [FreeRTOS-rust](https://github.com/lobaro/FreeRTOS-rust)는
C 커널 빌드와 Rust allocator 연결을 제공합니다. 2026-11-02 조회 기준 최근 commit은
[2025-12-18](https://github.com/lobaro/FreeRTOS-rust/commit/8b024f1174a0b40668d546e50c40b81a7df14a90)이며,
crate 배포 상태와 F446RE/V11.2.0 조합의 호환성은 확인하지 못했습니다.
이 프로젝트는 C 예제의 커널을 공유하고 필요한 함수만 C ABI로 노출합니다.

## 빌드·실행 흐름

CMake가 C 커널과 보드 코드를, Cargo가 Rust 앱과 로컬 `freertos-board` crate를 컴파일합니다.
GCC 링커가 이들을 하나의 ELF로 연결합니다.

```text
Reset_Handler → C main → board_init → xTaskCreate → vTaskStartScheduler
                                                        ↓
                                                  rust_entry → rust_task
```

Rust의 `delay_ms`는 C 래퍼를 거쳐 `vTaskDelay`를 호출합니다.
Hello World는 매초 UART 로그를 출력하고, Blink는 시작 로그 후 PA5를 매초 토글합니다.

## ABI와 unsafe

C 옵션은 `-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`,
Rust 타깃은 `thumbv7em-none-eabihf`입니다. 양쪽 모두 hard-float ABI를 사용합니다.
함수는 `extern "C"`로 연결하고 Rust 진입점은 `#[unsafe(no_mangle)]`로 내보냅니다.
`uint32_t`는 `u32`, `size_t`는 `usize`에 대응하며 이 타깃에서는 모두 32bit입니다.

`BoardTask::claim()`은 보드 초기화 후 앱 태스크에서 한 번만 호출하는 unsafe 함수입니다.
해당 태스크가 LED/UART를 독점해야 합니다. 토큰은 Send/Sync가 아니며 메서드는 `&mut self`를 받습니다.
C나 ISR이 같은 자원을 동시에 사용하지 않도록 해야 합니다.

UART 버퍼는 호출 중에만 빌려주며 C는 포인터를 보관하거나 수정하지 않습니다.
지연은 tick 환산 범위를 제한하도록 1~60000ms로 처리합니다. 이 API는 ISR에서 사용할 수 없습니다.

## panic과 메모리

Rust는 동적 할당을 사용하지 않습니다. 지역 변수는 FreeRTOS 태스크 스택에,
문자열 상수는 Flash에 놓입니다. C의 heap_4는 태스크와 TCB를 할당합니다.

`panic=abort`이며 공통 crate의 panic_handler가 `platform_fail(5)`를 호출합니다.
인터럽트를 끄고 진단 정보를 저장한 채 정지하며 C 경계로 unwind하지 않습니다.
다른 panic_handler를 추가하면 중복 정의가 됩니다.
