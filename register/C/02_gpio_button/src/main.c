#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"

_Noreturn void firmware_fail(uint32_t reason);

enum { POLL_MS = 10, DEBOUNCE_MS = 30 };

static void button_task(void *argument)
{
    (void)argument;
    uint32_t stable_pressed = 0;
    uint32_t candidate_pressed = 0;
    uint32_t unchanged_ms = 0;

    for (;;) {
        uint32_t pressed = (GPIOC->IDR & (1U << 13)) == 0U;
        if (pressed != candidate_pressed) {
            candidate_pressed = pressed;
            unchanged_ms = 0;
        } else if (unchanged_ms < DEBOUNCE_MS) {
            unchanged_ms += POLL_MS;
        }

        /* Debounce both edges; only a new stable press toggles the LED. */
        if (unchanged_ms >= DEBOUNCE_MS && stable_pressed != candidate_pressed) {
            stable_pressed = candidate_pressed;
            if (stable_pressed)
                GPIOA->BSRR = (GPIOA->ODR & (1U << 5)) ? (1U << 21) : (1U << 5);
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
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
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOC->MODER &= ~(3U << 26);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << 26)) | (1U << 26);

    if (xTaskCreate(button_task, "button", 256, NULL, 1, NULL) != pdPASS)
        firmware_fail(7);
    vTaskStartScheduler();
    firmware_fail(6);
}
