#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"

_Noreturn void firmware_fail(uint32_t reason);
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
    /* Internal HSI: 16 MHz, AHB/APB /1, independent of ST-LINK MCO. */
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
    GPIOA->BSRR = 1U << 21; /* PA5 initially LOW. */
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
    GPIOA->OTYPER &= ~(1U << 5);
    GPIOA->PUPDR &= ~(3U << 10);

    if (xTaskCreate(blink_task, "blink", 256, NULL, 1, NULL) != pdPASS)
        firmware_fail(7);
    vTaskStartScheduler();
    firmware_fail(6);
}
