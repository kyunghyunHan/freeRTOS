#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

_Noreturn void firmware_fail(uint32_t reason);
extern void rust_task(void) __attribute__((noreturn));

static void rust_entry(void *argument)
{
    (void)argument;
    rust_task();
}

int main(void)
{
    /* Internal HSI: 16 MHz, AHB/APB /1, independent of ST-LINK MCO. */
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
