# CubeIDE main.c 재사용

C 실습 코드는 `register/C/파일명.c` 하나로 작성합니다.
기존 C 소스는 내용을 바꾸지 않고 폴더 밖으로 옮겼습니다.
CMakeLists.txt는 `register/C/`에 하나만 있으며, `APP`로 컴파일할 파일을 선택합니다.
Rust 폴더 구조는 유지합니다.

## 복사와 빌드

CubeIDE의 main.c를 다음처럼 복사합니다. 대상 파일이 이미 있으면 먼저 내용을 보관하세요.

```bash
cp /path/to/CubeIDE/Core/Src/main.c register/C/main.c
cmake -S register/C -B register/C/build/main -G Ninja \
  -DAPP=main -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-gcc.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build register/C/build/main
```

`main.c`에는 레지스터 초기화, 태스크, xTaskCreate, vTaskDelay, vTaskStartScheduler,
main만 있어도 됩니다. FreeRTOS 오류 Hook이나 firmware_assert를 추가할 필요가 없습니다.
직접 주소 매크로 또는 ST CMSIS의 레지스터 정의를 사용할 수 있습니다. HAL GPIO 호출은 강제하지 않습니다.
예제별로 main 함수가 있으므로 선택한 `.c` 하나만 컴파일합니다.

버튼 예제는 `APP=02_gpio_button`, 결과는 `register/C/build/02_gpio_button/firmware.elf`,
`firmware.hex`, `firmware.bin`입니다. 같은 설정 후 코드만 수정하면 `cmake --build`로 재빌드합니다.

## 오류 원인과 기본 지원

기존 빌드는 Hook을 main.c에서 정의한다고 가정했습니다.
`configUSE_MALLOC_FAILED_HOOK=1`이므로 heap_4가 vApplicationMallocFailedHook을 참조하고,
`configCHECK_FOR_STACK_OVERFLOW=2`이므로 커널이 vApplicationStackOverflowHook을 참조합니다.
`configASSERT`는 firmware_assert를 호출합니다. main.c를 교체하면 해당 함수 정의가 사라져
커널 컴파일은 성공해도 최종 링크가 실패했습니다. 커널 자체 결함이나 heap 크기 문제는 아닙니다.

`cmake/freertos_hooks.c`를 실행 파일의 오브젝트로 자동 연결합니다.
다음 기본 함수는 weak 정의이므로 앱의 일반 함수 정의가 우선합니다.
일부 Hook만 앱에서 구현해도 나머지는 기본 함수가 채웁니다.

- firmware_assert(const char *file, int line)
- vApplicationMallocFailedHook(void): 설정이 1일 때
- vApplicationStackOverflowHook(TaskHandle_t task, char *name): 설정이 0보다 클 때
- firmware_fail(uint32_t reason), HardFault_Handler(void)

공통 기본 함수가 사용되면 인터럽트를 끄고 `g_freertos_diagnostic`을 저장한 채 정지합니다.
GDB에서 `p g_freertos_diagnostic`으로 확인합니다. reason은 1=할당 실패, 2=스택 넘침,
3=assert, 4=HardFault입니다. firmware_fail을 직접 호출하면 5=Rust panic, 6=scheduler 반환,
7=태스크 생성 실패 코드도 사용할 수 있습니다. file/line, task/name, IPSR/CFSR/HFSR/BFAR/MMFAR를 보존합니다.
오류 처리에서 UART나 heap을 사용하지 않습니다. 전체 예외 스택 프레임은 수집하지 않습니다.
앱이 함수를 재정의하면 그 앱의 오류 처리·진단 변수를 사용합니다. 기존 앱의 g_diagnostic도 유지됩니다.

앱이 추가 파일이나 정적 라이브러리로 Hook을 제공한다면 해당 오브젝트가 실제 링크되는지 확인해야 합니다.
`FIRMWARE_EXTRA_SOURCES`로 직접 소스를 추가하는 방식이 가장 명확합니다.
두 개의 strong 정의, 또는 앱과 기본 함수가 모두 weak인 구성은 자동 우선순위 보장 대상이 아닙니다.

## 실행 환경

| 항목 | 현재 설정 |
|---|---|
| MCU / ABI | STM32F446xx, cortex-m4, thumb, fpv4-sp-d16, hard-float |
| 툴체인 | Zephyr SDK 1.0.1 arm-zephyr-eabi-gcc 14.3.0, picolibc |
| 기본 클록 | 정상 하드웨어 리셋의 HSI 16MHz, 앱에서 변경 가능 |
| tick | 1000Hz, 32bit TickType_t |
| FreeRTOS | 고정 V11.2.0, GCC/ARM_CM4F 포트, heap_4 |
| heap | 32KiB, 동적 태스크 생성 |
| stack | C 예제 태스크 256 words=1024B, Rust 512 words=2048B, idle 최소 128 words |
| 메모리 | Flash 512KiB, RAM 124KiB + MSP 4KiB |
| HAL | 기본 빌드에 포함하지 않음 |

공통 `freertos_runtime.c`는 C 런타임 초기화 목록에서 VTOR를 실제 g_pfnVectors 주소로 설정하고
NVIC 우선순위 그룹을 0으로 지정합니다. 레지스터 주변장치 초기화는 수행하지 않습니다.
GCC의 `--wrap=vTaskStartScheduler`를 통해 scheduler 직전에 SystemCoreClockUpdate를 호출한 뒤
원래 FreeRTOS 함수로 진입합니다. 앱 소스나 커널 원본 수정 없이 현재 RCC 설정을 tick 계산에 반영합니다.
16MHz/1kHz일 때 SysTick LOAD는 15999입니다. scheduler 시작 후 클록 변경은 별도 tick 재설정이 필요합니다.

SVC/PendSV/SysTick은 FreeRTOS 포트가 제공하며 FreeRTOSConfig.h의 이름 매핑으로
ST startup 벡터에 연결합니다. CubeIDE의 stm32f4xx_it.c에 있는 같은 이름의 핸들러는 함께 추가하지 마세요.
포트의 strong 핸들러와 중복됩니다. HAL timebase와 SysTick을 함께 쓰려면 별도 타이머 timebase와
인터럽트 연결을 구성해야 하며, main.c 복사만으로 자동 구성되지 않습니다.

ST startup은 SystemInit으로 FPU를 활성화하고 .data/.bss를 초기화한 뒤 __libc_init_array와 main을 호출합니다.
SDK의 picolibc는 `__bothinit_array_start/end`를 사용하므로 공통 및 Rust 링커에 해당 심볼을 추가했습니다.
표준 newlib용 preinit/init/fini 배열 심볼도 유지했습니다. 현재 picolibc 링크에는 _init/_fini가 필요하지 않아
기본 구현을 추가하지 않았습니다. 앱에 기존 구현이 있어도 유지할 수 있습니다.
다른 libc로 툴체인을 교체할 경우 해당 런타임과 startup을 다시 검증해야 합니다.

## 프로젝트 설정 차이

C 기본 FreeRTOSConfig.h와 링커는 cmake/에 있습니다. Rust는 기존 앱별 설정을 사용합니다.
필요한 차이는 main.c 대신 CMake 옵션으로 지정합니다. 경로는 절대 경로 사용을 권장합니다.

| 옵션 | 용도 |
|---|---|
| APP | register/C/ 아래 확장자를 뺀 C 파일명 |
| FIRMWARE_CONFIG_DIR | 별도 FreeRTOSConfig.h가 있는 폴더 |
| FIRMWARE_LINKER_SCRIPT | 별도 링커 스크립트 |
| FIRMWARE_EXTRA_INCLUDE_DIRS | main.h 등 추가 헤더 경로, 세미콜론으로 여러 폴더 |
| FIRMWARE_EXTRA_SOURCES | HAL 드라이버, SystemClock_Config 등 추가 소스 목록 |
| FIRMWARE_EXTRA_DEFINITIONS | HSE_VALUE=8000000U 등 추가 매크로 |
| ARM_TOOLCHAIN_BIN | 동일한 arm-zephyr-eabi 접두사의 툴체인 bin 경로 |

예를 들어 main.c가 자체 main.h를 include하면 원본 헤더를 함께 보관하고 다음 옵션을 추가합니다.

```bash
-DFIRMWARE_EXTRA_INCLUDE_DIRS=/absolute/path/to/Core/Inc
```

외부 HSE를 쓰면 실제 입력 주파수와 HSE_VALUE를 맞춰야 합니다.
고정 ST CMSIS 소스의 기본 HSE_VALUE는 25MHz이므로 8MHz 입력 환경에서는 다음처럼 지정합니다.

```bash
-DFIRMWARE_EXTRA_DEFINITIONS=HSE_VALUE=8000000U
```

HAL_Init, HAL_GPIO_*, HAL_RCC_*, HAL_Delay를 호출하거나 main.h가 HAL 헤더를 include하는 코드는
HAL 드라이버, stm32f4xx_hal_conf.h, 필요한 모듈 소스를 추가해야 합니다.
CMSIS-RTOS의 osThreadNew/osDelay 등도 해당 어댑터가 별도로 필요합니다.
외부 프로젝트 고유의 pin 매크로·보조 함수·헤더를 추측해서 만들어 주지는 않습니다.
기존 startup, SystemInit, 커널, heap_4를 중복 추가하지 마세요.

GPIO 입력 극성·pull 설정·디바운스와 task stack 적정성은 애플리케이션 및 실제 회로에 달려 있습니다.
원본 C 소스를 그대로 보존하므로 이 빌드 지원이 애플리케이션 동작까지 바꾸지는 않습니다.

## 검증

```bash
python3 tests/verify_c_link.py
```

Hook 없는 단일 C, 일부 Hook만 재정의한 C, Hook 비활성 설정을 각각 컴파일·링크하고,
기본/앱 함수 선택, 인터럽트 벡터, 초기화 배열, scheduler 전 클록 갱신, hard-float ABI,
ELF/HEX/BIN 및 _init/_fini 불필요 여부를 확인합니다. 하드웨어 실행 테스트는 아닙니다.
실제 예제의 clean build 결과와 보드 접근 상태는 [검증 결과](verification.md)를 참고하세요.
