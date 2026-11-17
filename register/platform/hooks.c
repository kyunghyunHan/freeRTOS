#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"

/* 1=allocation, 2=stack overflow, 3=assert, 4=HardFault,
   5=Rust panic, 6=scheduler returned, 7=task creation. */
volatile struct {
    uint32_t reason, ipsr, cfsr, hfsr, bfar, mmfar;
    const char *file;
    int line;
    TaskHandle_t task;
    const char *task_name;
} g_diagnostic;

_Noreturn void platform_fail(uint32_t reason)
{
    __disable_irq();
    g_diagnostic.reason = reason;
    g_diagnostic.ipsr = __get_IPSR();
    g_diagnostic.cfsr = SCB->CFSR;
    g_diagnostic.hfsr = SCB->HFSR;
    g_diagnostic.bfar = SCB->BFAR;
    g_diagnostic.mmfar = SCB->MMFAR;
    /* No BKPT: this also halts safely when no debugger is connected. */
    for (;;) { __NOP(); }
}

void platform_assert(const char *file, int line)
{
    __disable_irq();
    g_diagnostic.file = file;
    g_diagnostic.line = line;
    platform_fail(3);
}
void vApplicationMallocFailedHook(void) { platform_fail(1); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    __disable_irq();
    g_diagnostic.task = task;
    g_diagnostic.task_name = name;
    platform_fail(2);
}
void HardFault_Handler(void) { platform_fail(4); }
