#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

extern const uint32_t g_pfnVectors[];
void __real_vTaskStartScheduler(void);

// main 실행 전 벡터와 기본 우선순위 설정
static void freertos_runtime_init(void)
{
    SCB->VTOR = (uint32_t)(uintptr_t)g_pfnVectors;
    __DSB();
    __ISB();
    NVIC_SetPriorityGrouping(0);
    SystemCoreClockUpdate();
}

// C 런타임의 초기화 목록
__attribute__((used, section(".preinit_array")))
static void (*const runtime_init)(void) = freertos_runtime_init;

// 앱에서 변경한 클록을 tick 설정에 반영
void __wrap_vTaskStartScheduler(void)
{
    SystemCoreClockUpdate();
    __real_vTaskStartScheduler();
}
