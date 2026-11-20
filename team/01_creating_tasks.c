
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
    RCC_AHB1ENR |= (1UL << 3);  /* GPIOD clock */
    RCC_APB1ENR |= (1UL << 18); /* USART3 clock */
    (void)RCC_APB1ENR;
    GPIOD_MODER = (GPIOD_MODER & ~(3UL << 16)) | (2UL << 16);
    GPIOD_AFRH = (GPIOD_AFRH & ~0xFUL) | 7UL; /* PD8 AF7 */
    USART3_CR1 = 0;
    USART3_BRR = 0x008BUL;                 /* 16 MHz / 115200 ~= 138.9 */
    USART3_CR1 = (1UL << 13) | (1UL << 3); /* UE, TE */
}

static void uart_print(const char *s)
{
    while (*s != '\0')
    {

        taskENTER_CRITICAL();
        while ((USART3_SR & (1UL << 7)) == 0U)
        {
        }
        USART3_DR = (uint8_t)*s++;
        taskEXIT_CRITICAL();
    }
}

static void start_scheduler_or_halt(void)
{
    vTaskStartScheduler();
    for (;;)
    {
    }
}

#define SPIN_COUNT 200000UL
static void task1(void *arg)
{
    (void)arg;
    for (;;)
    {
        uart_print("Task 1 is running\r\n");
        for (volatile uint32_t i = 0; i < SPIN_COUNT; ++i)
        {
        }
    }
}
static void task2(void *arg)
{
    (void)arg;
    for (;;)
    {
        uart_print("Task 2 is running\r\n");
        for (volatile uint32_t i = 0; i < SPIN_COUNT; ++i)
        {
        }
    }
}

/* 두 Task는 서로 다른 문자열을 출력하지만 구조는 거의 동일하다.
두 Task는 동일 Priority를 가지므로 선점형 Time Slicing 설정에서는 번갈아 실행 기회를 얻을 수 있다.
Tick 타이밍에 따라 한 Task가 여러 번 출력한 뒤 다른 Task가 실행될 수도 있다. */
int main(void)
{
    uart_init();
    if (xTaskCreate(task1, "Task1", 256, NULL, 1, NULL) != pdPASS)
        for (;;)
        {
        }
    if (xTaskCreate(task2, "Task2", 256, NULL, 1, NULL) != pdPASS)
        for (;;)
        {
        }
    start_scheduler_or_halt();
    return 0;
}
