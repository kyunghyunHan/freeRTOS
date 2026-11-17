#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"

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

static void blink_task(void *argument)
{
    (void)argument;
    for (;;) {
        GPIOA->BSRR = (GPIOA->ODR & (1U << 5)) ? (1U << 21) : (1U << 5);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->BSRR = 1U << 21; // LED 초기 꺼짐
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
    GPIOA->OTYPER &= ~(1U << 5);
    GPIOA->PUPDR &= ~(3U << 10);

    if (xTaskCreate(blink_task, "blink", 256, NULL, 1, NULL) != pdPASS)
        firmware_fail(7);
    vTaskStartScheduler();
    firmware_fail(6);
}
