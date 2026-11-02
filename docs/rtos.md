# FreeRTOS와 보드 원리

FreeRTOS는 실행 가능한 태스크 중 우선순위가 높은 태스크에 CPU를 할당합니다.

## 리셋에서 태스크까지

ST 공식 startup의 벡터 첫 항목은 MSP 초기값 0x20020000이고 Reset_Handler가 SystemInit을 호출합니다.
.data를 Flash에서 RAM으로 복사하고 .bss를 0으로 지운 뒤 C library init와 main을 호출합니다.
SystemInit은 FPU를 활성화합니다. 공통 board_init은 내부 HSI 16MHz와 AHB/APB prescaler /1을 명시합니다.
보드의 외부 크리스털·ST-LINK MCO에 의존하지 않습니다. PLL/180MHz 구성은 사용하지 않습니다.
STM32F446RE의 Flash 512KiB, SRAM 128KiB 전체를 링커에 선언하며 마지막 4KiB는 MSP 전용 영역으로 예약합니다.
.data/.bss가 124KiB RAM 영역을 넘으면 링크 오류가 납니다.

LD2는 PA5 active-high 출력, 초기 low입니다. PA2/PA3는 AF7 USART2이고 APB1=16MHz에서 BRR=139를 사용합니다.
PA13/PA14 SWD는 사용하지 않습니다. GPIO toggle은 단일 소유 태스크가 ODR을 읽고 BSRR로 갱신합니다.
여러 작성자가 동시에 toggle하면 읽기/갱신 조합은 원자적이지 않으므로 소유권 또는 잠금이 필요합니다.

C main은 xTaskCreate로 태스크를 만든 뒤 vTaskStartScheduler를 호출합니다.
Rust도 동일한 C main과 커널을 사용하고 태스크 진입점 안에서 rust_task를 호출합니다.

## 스케줄러와 tick

선점형 스케줄러, 최대 5개 우선순위(0~4), idle=0, 앱=1입니다. 숫자가 클수록 태스크 우선순위가 높습니다.
같은 우선순위의 Ready 태스크는 tick 기준 time slicing을 사용합니다.
SysTick은 코어 16MHz에서 1kHz로 실행하며 LOAD=15999입니다. tick은 32bit입니다.
FreeRTOS ARM_CM4F 포트가 SysTick을 설정하고 SVC로 첫 태스크를 시작하며 PendSV로 문맥을 전환합니다.

`vTaskDelay(pdMS_TO_TICKS(1000))`는 앱을 Blocked로 바꿔 CPU를 다른 Ready/idle 태스크에 내줍니다.
1000 tick 뒤 Ready가 되고 스케줄러가 선택하면 다시 실행합니다.
매번 실행 시점 기준 상대 지연이므로 작업 시간과 스케줄링 지연이 누적될 수 있습니다.
정밀한 주기 학습은 xTaskDelayUntil을 사용하세요. 매초 toggle은 완전한 켜짐/꺼짐 주기 약 2초입니다.
Idle은 기본 busy idle이고 tickless/저전력 WFI를 활성화하지 않았습니다.

## heap와 stack

heap_4는 .bss의 32KiB ucHeap을 관리하며 블록 할당/해제 및 인접 빈 블록 병합을 수행합니다.
이 예제는 동적 태스크 생성으로 앱 및 idle의 TCB/스택을 여기서 한 번 할당합니다.
C 앱 스택 256 words=1024B, Rust 512 words=2048B, idle 최소 128 words=512B입니다.
스택 크기의 단위는 bytes가 아니라 StackType_t(여기서는 4B) 개수입니다.
이 스택과 TCB가 이미 ucHeap 안에 있으므로 정적 RAM 사용량에 다시 더하지 않습니다.

태스크는 PSP, 리셋/예외/인터럽트는 MSP를 사용합니다. 마지막 4KiB의 MSP는 태스크 스택과 별개입니다.
uxTaskGetStackHighWaterMark(NULL)는 해당 태스크의 남은 최소 stack words를 관측하는 도구입니다.
MSP 여유는 별도로 확인해야 합니다.

configUSE_MALLOC_FAILED_HOOK=1, configCHECK_FOR_STACK_OVERFLOW=2, configASSERT를 활성화했습니다.
실패하면 IRQ를 끄고 g_diagnostic에 원인과 fault 레지스터를 저장한 채 정지합니다.
reason: 1 할당 실패, 2 스택 overflow, 3 assert, 4 HardFault, 5 Rust panic,
6 scheduler 반환, 7 태스크 생성 실패입니다. file/line/task/name도 해당 경로에서 저장합니다.
오류 처리에서 heap이나 UART를 사용하지 않습니다.
스택 검사는 문맥 전환 때 일부 패턴을 검사하므로 모든 손상이나 MSP overflow를 탐지하지 못합니다.
현재 fault 기록은 레지스터·원인 중심이고 전체 exception stack frame을 별도로 캡처하지 않습니다.

## 인터럽트 우선순위

STM32F446 NVIC는 4bit를 구현합니다. CMSIS에 전달하는 논리 우선순위는 0~15이며 **숫자가 작을수록 IRQ 우선순위가 높습니다**.
태스크 우선순위와 방향이 반대입니다. priority grouping=0(4 선점 bit)을 사용합니다.
커널 PendSV/SysTick은 15(0xF0), MAX_SYSCALL은 5(0x50)입니다.
논리 0~4의 IRQ는 FreeRTOS API를 호출하면 안 됩니다. 5~15에서만 해당 FromISR API를 사용하며
필요 시 portYIELD_FROM_ISR로 높은 우선순위 태스크를 깨웁니다.
ISR에서는 vTaskDelay/일반 mutex 대기 API를 호출할 수 없습니다. 현재 앱은 주변장치 IRQ를 사용하지 않습니다.

## Zephyr와 차이

| 항목 | 기존 Zephyr | 이 FreeRTOS 실습 |
|---|---|---|
| 관리 | west manifest/workspace | external lock + 직접 CMake |
| 보드 설정 | Devicetree/Kconfig | CMSIS + board.c + FreeRTOSConfig.h |
| 시작/링커 | 보드 빌드가 자동 구성 | ST startup + 공통 명시적 linker |
| 드라이버 | 커널과 통합된 장치 API | 직접 레지스터 초기화, 필요 시 HAL 추가 |
| 태스크 | 기본 main thread + k_thread | C main이 xTaskCreate/scheduler 시작 |
| 지연 | k_msleep | vTaskDelay |
| Rust | Zephyr 공식 모듈/CMake integration | 작은 로컬 C ABI + Rust staticlib |

출처: [공식 Cortex-M 포트 지침](https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/04-Demos/ARM-Cortex/RTOS-Cortex-M3-M4),
[고정 커널 소스](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/0adc196d4bd52a2d91102b525b0aafc1e14a2386),
[ST STM32F446RE](https://www.st.com/en/microcontrollers-microprocessors/stm32f446re.html).
