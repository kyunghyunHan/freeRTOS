#include "stm32f4xx.h"
#include "board.h"

void board_init(void)
{
    /* Deliberately use the internal 16 MHz HSI; no dependency on ST-LINK MCO. */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}
    RCC->CFGR = 0;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}
    RCC->CR &= ~RCC_CR_PLLON;
    SystemCoreClockUpdate();
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(0); /* Four preemption priority bits, no subpriority. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->BSRR = 1U << (5 + 16);
    GPIOA->MODER = (GPIOA->MODER & ~((3U << 10) | (3U << 4) | (3U << 6)))
                   | (1U << 10) | (2U << 4) | (2U << 6);
    GPIOA->OTYPER &= ~((1U << 5) | (1U << 2) | (1U << 3));
    GPIOA->PUPDR = (GPIOA->PUPDR & ~((3U << 10) | (3U << 4) | (3U << 6))) | (1U << 6);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((15U << 8) | (15U << 12))) | (7U << 8) | (7U << 12);
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = 0;
    USART2->BRR = (SystemCoreClock + 57600U) / 115200U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void board_led_toggle(void)
{
    /* Sole application task owns PA5. BSRR makes the actual update atomic. */
    GPIOA->BSRR = (GPIOA->ODR & (1U << 5)) ? (1U << 21) : (1U << 5);
}

void board_uart_write(const uint8_t *data, size_t len)
{
    /* Polling TX only, single task owner. No tick interrupt is disabled. */
    for (size_t i = 0; i < len; ++i) {
        while (!(USART2->SR & USART_SR_TXE)) {}
        USART2->DR = data[i];
    }
}
