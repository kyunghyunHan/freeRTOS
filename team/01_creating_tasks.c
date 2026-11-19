/*
 * Mastering FreeRTOS - Chapter 3, Example 1: Creating tasks
 * STM32F429ZI / NUCLEO-F429ZI / FreeRTOS V10.3.1 adaptation.
 * One standalone main.c-equivalent file. Do not compile all nine together.
 * UART: USART3 TX=PD8 AF7, 115200 8N1, APB1 clock assumed 16 MHz.
 * Use separately from CubeMX's generated main.c.
 */
#include <stdint.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define RCC_AHB1ENR REG32(0x40023830UL)
#define RCC_APB1ENR REG32(0x40023840UL)
#define GPIOD_MODER REG32(0x40020C00UL)
#define GPIOD_AFRH REG32(0x40020C24UL)
#define USART3_SR REG32(0x40004800UL)
#define USART3_DR REG32(0x40004804UL)
#define USART3_BRR REG32(0x40004808UL)
#define USART3_CR1 REG32(0x4000480CUL)

static void uart_init(void)
{
    RCC_AHB1ENR |= (1UL << 3);          /* GPIOD clock */
    RCC_APB1ENR |= (1UL << 18);         /* USART3 clock */
    (void)RCC_APB1ENR;
    GPIOD_MODER = (GPIOD_MODER & ~(3UL << 16)) | (2UL << 16);
    GPIOD_AFRH = (GPIOD_AFRH & ~0xFUL) | 7UL; /* PD8 AF7 */
    USART3_CR1 = 0;
    USART3_BRR = 0x008BUL;             /* 16 MHz / 115200 ~= 138.9 */
    USART3_CR1 = (1UL << 13) | (1UL << 3);  /* UE, TE */
}

static void uart_print(const char *s)
{
    while (*s != '\0')
    {
        /* Protect each TXE-check/DR-write pair from task preemption.
         * Individual messages may still interleave when priorities match.
         * This is a small educational UART, not a production logger. */
        taskENTER_CRITICAL();
        while ((USART3_SR & (1UL << 7)) == 0U) { }
        USART3_DR = (uint8_t)*s++;
        taskEXIT_CRITICAL();
    }
}

static void start_scheduler_or_halt(void)
{
    vTaskStartScheduler();
    for (;;) { }
}

/* Example 1: two task FUNCTIONS, same priority, busy-loop delay.
 * No vTaskDelay() here: both tasks stay runnable (Ready/Running).
 */
#define SPIN_COUNT 200000UL
static void task1(void *arg)
{
    (void)arg;
    for (;;) {
        uart_print("Task 1 is running\r\n");
        for (volatile uint32_t i = 0; i < SPIN_COUNT; ++i) { }
    }
}
static void task2(void *arg)
{
    (void)arg;
    for (;;) {
        uart_print("Task 2 is running\r\n");
        for (volatile uint32_t i = 0; i < SPIN_COUNT; ++i) { }
    }
}
int main(void)
{
    uart_init();
    if (xTaskCreate(task1, "Task1", 256, NULL, 1, NULL) != pdPASS) for (;;) { }
    if (xTaskCreate(task2, "Task2", 256, NULL, 1, NULL) != pdPASS) for (;;) { }
    start_scheduler_or_halt();
    return 0;
}
