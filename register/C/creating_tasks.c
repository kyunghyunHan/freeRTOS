
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F429ZI - FreeRTOS Example 1
 * Creating Tasks
 *
 * Task1 : Priority 1
 * Task2 : Priority 1
 *
 * Busy Waiting 사용
 * vTaskDelay() 사용하지 않음
 *
 * USART3 TX : PD8 (AF7)
 * Baudrate  : 115200
 * PCLK1     : 16MHz
 *
 * configUSE_PREEMPTION = 1
 * configUSE_TIME_SLICING = 1
 */

/* =========================================
 * 1. Register Address
 * ========================================= */

#define RCC_BASE       0x40023800UL
#define GPIOD_BASE     0x40020C00UL
#define USART3_BASE    0x40004800UL

#define RCC_AHB1ENR \
    (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

#define RCC_APB1ENR \
    (*(volatile uint32_t *)(RCC_BASE + 0x40UL))

#define GPIOD_MODER \
    (*(volatile uint32_t *)(GPIOD_BASE + 0x00UL))

#define GPIOD_AFRH \
    (*(volatile uint32_t *)(GPIOD_BASE + 0x24UL))

#define USART3_SR \
    (*(volatile uint32_t *)(USART3_BASE + 0x00UL))

#define USART3_DR \
    (*(volatile uint32_t *)(USART3_BASE + 0x04UL))

#define USART3_BRR \
    (*(volatile uint32_t *)(USART3_BASE + 0x08UL))

#define USART3_CR1 \
    (*(volatile uint32_t *)(USART3_BASE + 0x0CUL))

/* =========================================
 * 2. UART 초기화
 * ========================================= */

static void UART3_Init(void)
{
    /* GPIOD, USART3 Clock Enable */
    RCC_AHB1ENR |= (1UL << 3);
    RCC_APB1ENR |= (1UL << 18);

    (void)RCC_AHB1ENR;

    /* PD8 Alternate Function */
    GPIOD_MODER &= ~(3UL << 16);
    GPIOD_MODER |=  (2UL << 16);

    /* PD8 AF7 */
    GPIOD_AFRH &= ~(0xFUL << 0);
    GPIOD_AFRH |=  (7UL << 0);

    /* 115200bps, PCLK1 = 16MHz */
    USART3_BRR = 0x008B;

    /* USART Enable + TX Enable */
    USART3_CR1 = (1UL << 13) | (1UL << 3);
}

/* =========================================
 * 3. UART 출력
 * ========================================= */

static void UART3_SendChar(char c)
{
    while ((USART3_SR & (1UL << 7)) == 0U)
    {
    }

    USART3_DR = (uint8_t)c;
}

static void UART3_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART3_SendChar(*str++);
    }
}

/* =========================================
 * 4. Task1
 * ========================================= */

static void Task1(void *argument)
{
    (void)argument;

    for (;;)
    {
        UART3_SendString("Task1 Running\r\n");

        /*
         * Busy Waiting
         *
         * CPU를 사용해 반복문 수행
         * Blocked 상태로 전환되지 않음
         */
        for (volatile uint32_t i = 0;
             i < 1000000UL;
             i++)
        {
        }
    }
}

/* =========================================
 * 5. Task2
 * ========================================= */

static void Task2(void *argument)
{
    (void)argument;

    for (;;)
    {
        UART3_SendString("Task2 Running\r\n");

        /* Busy Waiting */
        for (volatile uint32_t i = 0;
             i < 1000000UL;
             i++)
        {
        }
    }
}

/* =========================================
 * 6. main()
 * ========================================= */

int main(void)
{
    BaseType_t result1;
    BaseType_t result2;

    UART3_Init();

    UART3_SendString("\r\nExample 1 - Creating Tasks\r\n");

    /* Task1 생성 */
    result1 = xTaskCreate(
        Task1,
        "Task1",
        256,
        NULL,
        1,
        NULL
    );

    /* Task2 생성 */
    result2 = xTaskCreate(
        Task2,
        "Task2",
        256,
        NULL,
        1,
        NULL
    );

    /* 생성 실패 확인 */
    if ((result1 != pdPASS) ||
        (result2 != pdPASS))
    {
        UART3_SendString("Task Creation Failed\r\n");

        while (1)
        {
        }
    }

    /* Scheduler 시작 */
    vTaskStartScheduler();

    while (1)
    {
    }
}
