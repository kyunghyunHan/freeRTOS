# Rust와 C 연결

Rust 애플리케이션은 `core`만 사용하는 `no_std` 코드입니다.
모든 Rust 예제는 Cargo가 직접 링크하는 실행 파일입니다.
`01_hello_world`·`02_led_blink`·`02_gpio_button`의 출력 이름은 `firmware`입니다.
모든 예제의 main과 태스크는 Rust로 작성하며 예제 폴더에 C 소스는 없습니다.
FreeRTOS 커널과 CMSIS·공통 런타임은 C/ASM으로 연결합니다.
GPIO/UART 초기화와 제어는 `src/lib.rs`에서 실제 레지스터 주소에 직접 접근합니다.

## 구성

| 파일 | 역할 |
|---|---|
| register/Rust/support/startup.rs | LED·버튼 예제의 HSI 16MHz, 태스크·스케줄러 시작(Rust) |
| 각 예제의 src/lib.rs | Rust main, 레지스터 접근, 태스크 로직, panic_handler |
| 각 예제의 FreeRTOSConfig.h | 1kHz tick, 32bit TickType_t, heap/인터럽트 설정 |
| 각 예제의 stm32f446re.ld | Flash/RAM/MSP 메모리 배치 |
| register/Rust/support/cargo_build.rs | Cargo 예제의 C 커널·CMSIS 빌드와 ELF 링크 설정 |
| cmake/firmware.cmake | C 예제 빌드 |
| cmake/cargo_firmware.cmake | F429ZI Cargo 예제의 선택적 CMake 호환 진입점 |
| cmake/freertos_hooks.c | 앱에 없는 오류 처리 기본 함수 |
| cmake/freertos_runtime.c | 벡터 초기화, scheduler 직전 클록 갱신 |

## 실행 흐름

```text
Reset_Handler → Rust main → xTaskCreate → vTaskStartScheduler
                                              ↓
                                        Rust 태스크
                                              ↓
                                  GPIO/UART 레지스터 직접 접근
```

C에서 보드 제어 함수를 호출하는 계층은 없습니다. Rust가 RCC/GPIO/USART 주소에
`read_volatile` / `write_volatile`을 수행합니다. 한 앱 태스크가 해당 주변장치를 독점합니다.
Cargo Hello World(F429ZI)는 USART3 PD8/PD9로 매초 문자열을 출력합니다.
`01_hello_world`는 사용자가 작성한 F429ZI LED·버튼 코드입니다.
Blink(F429ZI)는 시작 로그 후 LD1(PB0)을 매초 토글합니다.
F429ZI 버튼 예제는 PC13의 HIGH를 눌림으로 읽고 10ms 폴링·30ms 디바운스 후 LD1(PB0)을 토글합니다.

## ABI와 unsafe

C 옵션은 `-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`,
Rust 타깃은 `thumbv7em-none-eabihf`입니다. 양쪽 모두 hard-float ABI를 사용합니다.
Rust 진입점은 `#[unsafe(no_mangle)]`와 `extern "C"`로 내보냅니다.

Rust는 커널의 `vTaskDelay(u32)`를 직접 호출합니다. 이 선언은 각 예제의
`FreeRTOSConfig.h`에 있는 32bit tick, 1000Hz 설정에 맞춘 것으로 1 tick이 1ms입니다.
이 설정을 바꾸면 Rust 선언과 지연값도 함께 변경해야 합니다. ISR에서는 호출할 수 없습니다.

보드별 예제는 STM32F446 또는 STM32F429의 레지스터를 사용하며 정렬된 32bit 접근입니다. volatile은 실제 I/O 접근을
보장하지만 태스크 간 동기화는 제공하지 않습니다. 현재는 한 태스크만 GPIO/UART를 사용합니다.
여러 태스크나 ISR을 추가하면 같은 레지스터의 읽기-수정-쓰기 경합을 방지해야 합니다.

## panic과 메모리

Rust는 동적 할당을 사용하지 않습니다. 지역 변수는 FreeRTOS 태스크 스택에,
문자열 상수는 Flash에 놓입니다. C의 heap_4는 태스크와 TCB를 할당합니다.

LED·버튼·Cargo Hello World의 panic_handler는 공통 `firmware_fail(5)`를 호출합니다.
진단 변수는 `g_freertos_diagnostic`입니다. 사용자 코드가 오류 함수를 재정의하면 그 정의가 우선합니다.
인터럽트를 끄고 진단 정보를 저장한 채 정지하며 C 경계로 unwind하지 않습니다.
