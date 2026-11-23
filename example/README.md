# STM32F429ZI FreeRTOS Native API 학습 예제

NUCLEO-F429ZI의 Chapter 3~9, Example 01~25를 각 파일 하나의 독립 애플리케이션으로 구성했다. 번호·파일 구분, 기존 태스크 생성 순서, 우선순위, 큐 용량, 지연·타이머 주기는 유지했다. 원본 자체가 2016 교재의 새 각색이라고 명시되어 있었으므로, 외부 교재의 원문과 동일하다고 보장하는 자료는 아니다. 이 저장소의 기존 동작을 변환 기준으로 삼았다.

`exampleXX.c` 안에 main, 레지스터 초기화, 태스크, ISR, 오류 hook이 모두 있다. `example_bsp.c`와 HAL/CMSIS-RTOS 시작 안내 파일을 제거했다. 별도 BSP 헤더나 example_start 호출은 필요 없다. 다른 폴더의 Rust/F446RE 실습은 이 변환 대상이 아니며 기존 상태를 유지한다.

## 실행 환경과 파일

| 파일 | 역할 |
|---|---|
| `example01.c`~`example25.c` | 선택한 파일 하나만 빌드. CubeIDE에서는 main.c로 사용 가능 |
| `FreeRTOSConfig.h` | 이 실습 전용 커널 설정. 기존 cmake/ 설정과 혼용 금지 |
| `CMakeLists.txt` | APP 선택, ST startup/SystemInit, 커널 원본과 ARM_CM4F 포트 링크 |
| `arm-none-eabi.cmake` | CubeIDE의 GCC/newlib 툴체인 선택 |
| `Makefile` | CubeIDE의 Makefile project에서 같은 빌드 실행 |
| `newlib_init.c` | newlib의 빈 `_init`/`_fini`, C heap을 비활성화하는 `_sbrk`. BSP 기능 없음 |
| `../cmake/stm32f429zi.ld` | 기존 2MiB Flash / SRAM192KiB(마지막4KiB는 MSP) 링커, 변경 없음 |

각 소스의 `hardware_init`, `uart_write`는 해당 파일 안의 static 함수이다. 단일 파일을 읽어 실습할 수 있도록 작은 초기화/출력 코드를 각 파일에 명시했다. 별도의 공통 실습 프레임워크는 없다. 오류 hook도 각 파일에서 제공하므로 `cmake/freertos_hooks.c`, `freertos_runtime.c`를 중복 연결하지 않는다.

HSI를 선택하고 AHB/APB prescaler를 1로 만들어 SYSCLK/HCLK/PCLK1/PCLK2=16MHz로 맞춘다. PLL/HSE는 사용하지 않는다. 외부 발진기나 ST-LINK MCO 설정에 의존하지 않는다. SystemCoreClockUpdate 후 scheduler가 SysTick LOAD=15999(1kHz)를 설정한다. scheduler 시작 후 클록을 변경하지 않는다.

USART3 PD8/PD9 AF7, 115200 8N1이며 BRR=139이다. 정격 HSI에서 실제 baud는 약115108(-0.08%)이며 HSI 자체 오차는 별도이다. ST-LINK Virtual COM Port에서 확인한다. RX는 활성화하지만 수신 실습은 포함하지 않는다. PB0/7/14는 push-pull output이며 초기값 OFF이다. PB0는 토글 실습, PB14는 오류 표시로 사용하고 PB7는 출력 설정만 한다.

버튼은 PC13 입력/pull-down, 눌림 상승 에지이며 EXTI13을 PC13에 연결한다. IRQ 우선순위=6, grouping=0(선점4비트), FreeRTOS API 허용 IRQ 범위=5..15이다. EXTI15_10에서 PR13만 W1C로 지우며 scheduler 시작 전 이벤트는 버린다. 기계식 버튼의 bounce를 그대로 전달하므로 한 번 눌러 여러 이벤트가 생길 수 있다. Binary semaphore는 합쳐지고 counting semaphore/queue는 용량이 차면 추가 이벤트를 잃는다. 타이머 daemon 큐가 차면 deferred 요청도 실패할 수 있다. ISR에서는 UART/일반 semaphore API/지연 함수를 호출하지 않는다.

## 빌드와 실행

프로젝트 루트에서 기존 설치된 SDK로 빌드:

```sh
cmake -S example -B example/build/example01 -G Ninja \
  -DAPP=example01 -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build example/build/example01
```

CubeIDE GCC 또는 PATH의 arm-none-eabi-gcc 사용:

```sh
cmake -S example -B example/build/cube/example01 -G Ninja \
  -DAPP=example01 -DCMAKE_TOOLCHAIN_FILE="$PWD/example/arm-none-eabi.cmake" \
  -DARM_GCC_BIN="/absolute/path/to/STM32CubeIDE/tools/bin" -DCMAKE_BUILD_TYPE=Debug
cmake --build example/build/cube/example01
```

APP를 example02~example25로 바꾸고 빌드 디렉터리도 각각 분리한다. ELF/HEX/BIN/MAP는 선택한 build 디렉터리에 생성된다. EXAMPLE_NUMBER는 APP에서 계산하여 커널과 앱 전체에 전달한다.

CubeIDE에서 바로 빌드하려면 `File > New > Makefile Project with Existing Code`로 `example/`를 선택하고 ARM cross toolchain을 사용한다. C/C++ Build의 build command를 `make`로 지정하고 build target에 `APP=example16 ARM_GCC_BIN="/absolute/path/to/tools/bin"`처럼 설정한다. IDE 환경 PATH에 cmake, ninja, make가 필요하다. Debug Configuration의 MCU는 STM32F429ZITx, ELF는 `example/build/cubeide/example16/firmware.elf`, probe는 ST-LINK로 지정한다. 프로젝트 경로를 옮겨도 이 Makefile과 상대 소스 경로를 사용할 수 있다.

기존 CubeIDE managed project에 옮길 경우:

1. STM32F429ZITx의 Empty C 프로젝트를 만들고 ST의 `startup_stm32f429xx.s`, `system_stm32f4xx.c`와 F429ZI 링커를 유지한다. 기존 프로젝트를 쓸 때에도 startup과 SystemInit은 한 개만 링크한다.
2. 선택한 exampleXX.c를 Core/Src/main.c로 사용하고 이 폴더의 FreeRTOSConfig.h를 include 경로 맨 앞에 둔다. Preprocessor Symbols에 `STM32F429xx`, `EXAMPLE_NUMBER=16`처럼 번호를 **앱과 커널 전체에** 설정한다.
3. CMSIS Core/Device 헤더, FreeRTOS include/portable/GCC/ARM_CM4F include 경로를 추가한다. CMakeLists.txt에 열거된 tasks/queue/list/timers/event_groups/port/heap_4 소스를 한 번씩 추가한다. 포트는 GCC ARM_CM4F, ABI는 cortex-m4/thumb/fpv4-sp-d16/hard이다.
4. HAL_Init, SystemClock_Config, MX_*, CMSIS-RTOS adapter, 생성된 freertos.c는 빌드하지 않는다. main.c에 필요한 레지스터 초기화가 있다. `stm32f4xx_it.c`의 SysTick/PendSV/SVC 및 EXTI 중복 정의도 빌드에서 제외한다. 세 RTOS 핸들러는 FreeRTOSConfig의 이름 매핑을 통해 **원래 port.c**에서 연결한다.
5. 이 빌드에서 검증한 링커는 `../cmake/stm32f429zi.ld`이다. CubeIDE 생성 링커를 유지해도 되지만 Flash 시작=0x08000000, SRAM/MSP 배치와 startup용 심볼을 확인한다. bootloader offset은 지원하지 않는다.
6. newlib에서는 `newlib_init.c`를 추가한다. `_init`/`_fini`는 빈 초기화/종료 함수이며 `_sbrk`는 ENOMEM을 반환하여 libc heap을 비활성화한다. 정수 snprintf에는 libc heap이 필요하지 않다. 기존 런타임이 제공하는 같은 이름의 함수와 중복 추가하지 않는다. nano/nosys specs를 사용하며 printf retarget은 필요 없다. 선택 파일이 hook을 제공하므로 공통 hook 파일을 추가하지 않는다.

Flash 후 reset하고 터미널을 열면 출력이 보인다. 예13은 시작 시 한 번만 나타나는 출력이 있으므로 터미널을 먼저 연결하는 편이 좋다. IRQ 실습은 B1 버튼을 누른다. 실제 ST-LINK 연결과 실행은 별도 하드웨어 검증이 필요하다.

## 예제별 학습과 예상 동작

모든 파일 경로는 아래 표의 링크이며 실행 선택은 해당 파일명에서 `.c`를 뺀 APP이다. 공통 설정은 preemption=1, time slicing=1, tick=1ms, heap_4=32KiB, MAX_PRIORITIES=5이다. 애플리케이션 stack=512 words(2048B), idle=128 words, daemon=512 words/priority4/queue16이다. 문자열 formatting의 여유를 위해 기존256 words를512로 늘렸으며 실제 stack high-water mark는 보드에서 확인해야 한다. 우선순위 숫자가 큰 태스크가 먼저 실행된다.

| Chapter / 파일 | 실행·관찰 순서 / 추가 설정 |
|---|---|
| 3 / [example01.c](example01.c) | 동일 priority1 Task1/2 생성; 400/700ms 지연으로 교대 |
| 3 / [example02.c](example02.c) | 같은 함수에 A/B 문자열 전달, 둘 다1, 500ms |
| 3 / [example03.c](example03.c) | low1을 먼저 만들어도 high2가 먼저 실행, 각각300ms |
| 3 / [example04.c](example04.c) | priority2, PB0 toggle 후500ms Blocked |
| 3 / [example05.c](example05.c) | priority2, DelayUntil로500ms 절대 주기 |
| 3 / [example06.c](example06.c) | periodic2의300ms 대기 동안 continuous1이 계속 실행 |
| 3 / [example07.c](example07.c) | reporter1이1초 대기하는 동안 Idle hook 증가; 이 예제만 configUSE_IDLE_HOOK=1 |
| 3 / [example08.c](example08.c) | manager2가 worker1→3→1 변경, 변경 간격1200ms |
| 3 / [example09.c](example09.c) | killer2가1300ms 후 temporary1 삭제, 이어 자기삭제 |
| 4 / [example10.c](example10.c) | receiver2가 빈 큐에서 대기, sender1이400ms마다 전송 |
| 4 / [example11.c](example11.c) | sender2/receiver1, 구조체 복사, 길이2 큐가 차면 최대700ms 송신 대기 |
| 4 / [example12.c](example12.c) | producer1/consumer2, 길이3 큐2개/set6; configUSE_QUEUE_SETS=1 |
| 5 / [example13.c](example13.c) | daemon4 callback, one-shot1500ms/auto500ms |
| 5 / [example14.c](example14.c) | daemon4, ID1/2를700/1100ms callback에서 읽음 |
| 5 / [example15.c](example15.c) | activity2가500ms마다5회 reset, 마지막 reset+2초에 timeout |
| 6 / [example16.c](example16.c) | PC13 IRQ→binary semaphore→worker3, PB0 toggle |
| 6 / [example17.c](example17.c) | IRQ→counting semaphore(max10)→worker2,200ms마다 소비 |
| 6 / [example18.c](example18.c) | IRQ→xTimerPendFunctionCallFromISR→daemon4, worker 없음 |
| 6 / [example19.c](example19.c) | IRQ→값13을 queue8에 복사→receiver3 |
| 7 / [example20.c](example20.c) | writer A/B 모두1, mutex로 전체 문자열 보호,200ms |
| 7 / [example21.c](example21.c) | client A/B1→queue10→gatekeeper3만 UART 출력,350ms |
| 8 / [example22.c](example22.c) | setter1→300ms마다ABC, OR2는 첫 비트/AND2는 모두 기다림 |
| 8 / [example23.c](example23.c) | A/B/C 모두2,250/500/1000ms 후 sync barrier, 모두 도착하면 통과 |
| 9 / [example24.c](example24.c) | IRQ→notification→worker3, pdTRUE로 누적 count 전체 clear |
| 9 / [example25.c](example25.c) | IRQ 한 번→notify3회→worker3이150ms 간격으로 하나씩 소비 |

Ready는 실행 가능 상태, Running은 CPU를 사용하는 상태, Blocked는 시간이나 객체를 기다리는 상태이다. 지연/queue/semaphore/event/notify 조건이 충족되면 Ready가 되고 우선순위에 따라 Running으로 선택된다. 동일 우선순위에서의 출력 순서와 같은 tick에 만료하는 timer의 출력 순서는 일반적으로 고정하지 않는다. Example08도 worker가 Blocked라면 priority 변경만으로 즉시 실행되지는 않는다.

기본은 선점형이다. time slicing을0으로 바꾸면 같은 우선순위의 tick 강제 교대가 사라지지만 높은 우선순위 태스크의 선점은 유지된다. configUSE_PREEMPTION=0은 협력형으로, 태스크의 block/yield가 필요하며 Example06의 continuous는 CPU를 놓지 않으므로 periodic 재실행이 지연된다. 이 비교는 별도의 설정 변경 실험이며 기본 파일에서는 원래 실행 조건을 유지했다. 설정 변경 시 반드시 커널도 다시 빌드한다.

ISR의 FromISR API는 객체를 갱신하고 더 높은 우선순위 태스크가 Ready가 되었는지 woken에 기록한다. portYIELD_FROM_ISR가 PendSV를 요청하고, IRQ 복귀 시 포트가 문맥을 저장·복원하여 태스크를 바꾼다. ISR 안에서 바로 일반 태스크 함수를 실행하는 방식이 아니다. daemon deferred callback과 software timer callback은 priority4 태스크 문맥이다. 콜백에서 FreeRTOS 대기를 하지 않으며 이 예제의 UART 폴링도 daemon을 수 ms 지연시킬 수 있다.

UART의 TXE 검사/DR 쓰기는 짧은 critical section으로 한 문자를 원자적으로 처리한다. TXE를 기다리는 동안에는 critical section 밖에 있으므로 SysTick/태스크 전환을 막지 않는다. 이것은 전체 문자열 보호와 다르다. 일반 예제의 문자열은 섞일 수 있고 Example20의 mutex와 Example21의 gatekeeper에서 메시지 단위 보호 효과를 비교할 수 있다. mutex를 IRQ에서 사용하지 않는다. Example20은 원래 두 writer가 모두 priority1이므로 priority inheritance를 실제 유발하지는 않는다. 높은 우선순위 태스크가 mutex에서 기다릴 때 낮은 소유자의 우선순위가 상속된다는 원리만 주석으로 설명하며, 이를 강제로 보여주기 위해 원래 태스크 구성은 바꾸지 않았다.

## 검증

```sh
python3 tests/verify_f429_examples.py
python3 tests/verify_f429_examples.py --arm-gcc-bin "/absolute/path/to/STM32CubeIDE/tools/bin"
```

스크립트는 전체25개를 각각 컴파일·링크하고 ELF/HEX/BIN, main/SystemCoreClock, 초기 MSP=0x20030000, SVC/PendSV/SysTick 및 해당 EXTI 벡터의 실제 핸들러 주소를 검사한다. HAL/CMSIS-RTOS/BSP 심볼의 부재도 확인한다. 결과는 `example/build/sdk/verification.json`, `example/build/cube/verification.json`과 각 빌드 폴더의 verification.log에 저장한다.

2026-11-23 검증 결과:

| 검사 | 결과 |
|---|---|
| 기존 SDK GCC14.3.0/picolibc | 25/25 컴파일·링크 및 벡터/심볼/산출물 검사 PASS |
| STM32CubeIDE 내장 GCC14.3.1/newlib-nano | 25/25 컴파일·링크 및 벡터/심볼/산출물 검사 PASS |
| 컴파일 옵션 | 두 환경 모두 -Wall -Wextra -Werror, Debug |
| 원본 대비 태스크 생성 | 25개 생성 순서/인수/우선순위 동일, stack만256→512 words |
| 원본 대비 pdMS_TO_TICKS 인수 | 25개 모두 동일 |
| 커널/포트/startup/기존 링커 변경 | 없음 |

빌드 검증은 보드 실행, UART baud 실측, 버튼 bounce/LED, stack 최대사용량, 실제 선점 타이밍을 증명하지 않는다. 해당 항목은 보드에서 별도 확인한다. CubeIDE GUI의 import/debug 세션도 별도이다.

레지스터 근거: ST [RM0090](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)의 RCC/GPIO/SYSCFG/EXTI/USART 장, 보드 배선은 [UM1974](https://www.st.com/resource/en/user_manual/dm00244518-stm32-nucleo-144-boards-mb1137-stmicroelectronics.pdf). 비트 정의는 저장소의 `external/cmsis-device-f4/Include/stm32f429xx.h`를 사용한다. FreeRTOS 커널·CMSIS·startup·기존 링커 원본은 수정하지 않았다.
