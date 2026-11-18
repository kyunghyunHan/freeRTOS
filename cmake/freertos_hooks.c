#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

// 공통 오류 기록
volatile struct {
    uint32_t reason, ipsr, cfsr, hfsr, bfar, mmfar;
    const char *file;
    int line;
    TaskHandle_t task;
    const char *task_name;
} g_freertos_diagnostic;

// 앱의 같은 이름 함수가 우선
__attribute__((weak, noreturn)) void firmware_fail(uint32_t reason)
{
    __disable_irq();
    g_freertos_diagnostic.reason = reason;
    g_freertos_diagnostic.ipsr = __get_IPSR();
    g_freertos_diagnostic.cfsr = SCB->CFSR;
    g_freertos_diagnostic.hfsr = SCB->HFSR;
    g_freertos_diagnostic.bfar = SCB->BFAR;
    g_freertos_diagnostic.mmfar = SCB->MMFAR;
    for (;;) { __NOP(); }
}

__attribute__((weak)) void firmware_assert(const char *file, int line)
{
    __disable_irq();
    g_freertos_diagnostic.file = file;
    g_freertos_diagnostic.line = line;
    firmware_fail(3);
}

#if configUSE_MALLOC_FAILED_HOOK == 1
__attribute__((weak)) void vApplicationMallocFailedHook(void)
{
    firmware_fail(1);
}
#endif

#if configCHECK_FOR_STACK_OVERFLOW > 0
__attribute__((weak)) void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    __disable_irq();
    g_freertos_diagnostic.task = task;
    g_freertos_diagnostic.task_name = name;
    firmware_fail(2);
}
#endif

__attribute__((weak)) void HardFault_Handler(void)
{
    firmware_fail(4);
}
