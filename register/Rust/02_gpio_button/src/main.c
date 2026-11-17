#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

// 오류 원인
volatile struct {
    uint32_t reason, ipsr, cfsr, hfsr, bfar, mmfar;
    const char *file;
    int line;
    TaskHandle_t task;
    const char *task_name;
} g_diagnostic;

_Noreturn void firmware_fail(uint32_t reason)
{
    __disable_irq();
    g_diagnostic.reason = reason;
    g_diagnostic.ipsr = __get_IPSR();
    g_diagnostic.cfsr = SCB->CFSR;
    g_diagnostic.hfsr = SCB->HFSR;
    g_diagnostic.bfar = SCB->BFAR;
    g_diagnostic.mmfar = SCB->MMFAR;
    // 오류 시 정지
    for (;;) { __NOP(); }
}

void firmware_assert(const char *file, int line)
{
    __disable_irq();
    g_diagnostic.file = file;
    g_diagnostic.line = line;
    firmware_fail(3);
}
void vApplicationMallocFailedHook(void) { firmware_fail(1); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    __disable_irq();
    g_diagnostic.task = task;
    g_diagnostic.task_name = name;
    firmware_fail(2);
}
void HardFault_Handler(void) { firmware_fail(4); }

// C 런타임 초기화
void _init(void) {}
void _fini(void) {}

extern void rust_task(void) __attribute__((noreturn));

static void rust_entry(void *argument)
{
    (void)argument;
    rust_task();
}

int main(void)
{
    // HSI 16MHz 설정
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}
    RCC->CFGR = 0;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}
    RCC->CR &= ~RCC_CR_PLLON;
    SystemCoreClockUpdate();
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(0);
    if (xTaskCreate(rust_entry, "rust", 512, NULL, 1, NULL) != pdPASS)
        firmware_fail(7);
    vTaskStartScheduler();
    firmware_fail(6);
}
