
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#define RCC_BASE 0x40023800UL
#define GPIOD_BASE 0x40020C00UL
#define USART3_BASE 0x40004800UL

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

static void UART3_Init(void)
{
    /* GPIOD Clock Enable */
    RCC_AHB1ENR |= (1UL << 3);

    /* USART3 Clock Enable */
    RCC_APB1ENR |= (1UL << 18);

    /* PD8 = Alternate Function */
    GPIOD_MODER &= ~(3UL << 16);
    GPIOD_MODER |= (2UL << 16);

    /* PD8 = AF7 (USART3 TX) */
    GPIOD_AFRH &= ~(0xFUL << 0);
    GPIOD_AFRH |= (7UL << 0);

    /*
     * USART3
     * PCLK1 = 16MHz
     * Baudrate = 115200
     * Oversampling by 16
     *
     * BRR = 0x008B
     */
    USART3_BRR = 0x008B;

    /* UE = 1, TE = 1 */
    USART3_CR1 = (1UL << 13) | (1UL << 3);
}

static void UART3_SendChar(char c)
{
    /* TXE = 1이 될 때까지 대기 */
    while ((USART3_SR & (1UL << 7)) == 0U)
    {
    }

    USART3_DR = (uint8_t)c;
}

static void UART3_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART3_SendChar(*str);
        str++;
    }
}

static void Task1(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* Task1 실행 확인 */
        UART3_SendString("Task1 Running\r\n");

        /*
         * Busy Waiting
         *
         * CPU가 빈 반복문을 계속 실행
         * Task는 Blocked 상태로 가지 않음
         *
         * configUSE_PREEMPTION = 1
         * configUSE_TIME_SLICING = 1이면
         * 같은 Priority의 Task끼리
         * Tick 기반으로 번갈아 실행 가능
         */
        for (volatile uint32_t i = 0;
             i < 1000000UL;
             i++)
        {
        }
    }
}

static void Task2(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* Task2 실행 확인 */
        UART3_SendString("Task2 Running\r\n");

        /* Busy Waiting */
        for (volatile uint32_t i = 0;
             i < 1000000UL;
             i++)
        {
        }
    }
}

int main(void)
{
    BaseType_t result1;
    BaseType_t result2;

    /* USART3 초기화 */
    UART3_Init();

    UART3_SendString("\r\n");
    UART3_SendString("FreeRTOS Example 1\r\n");
    UART3_SendString("Creating Two Tasks\r\n");

    /*
     * Task1 생성
     *
     * Task Function = Task1
     * Stack = 256 words
     * Priority = 1
     */
    result1 = xTaskCreate(
        Task1,
        "Task1",
        256,
        NULL,
        1,
        NULL);

    /*
     * Task2 생성
     *
     * Task Function = Task2
     * Stack = 256 words
     * Priority = 1
     */
    result2 = xTaskCreate(
        Task2,
        "Task2",
        256,
        NULL,
        1,
        NULL);

    /* Task 생성 성공 여부 확인 */
    if ((result1 != pdPASS) ||
        (result2 != pdPASS))
    {
        UART3_SendString("Task Creation Failed\r\n");

        while (1)
        {
        }
    }

    UART3_SendString("Starting Scheduler\r\n");

    /*
     * FreeRTOS Scheduler 시작
     *
     * 이후 Task1, Task2가 실행됨
     */
    vTaskStartScheduler();

    /*
     * 정상적으로 시작된 Scheduler는
     * 일반적으로 이 위치로 돌아오지 않음
     */
    while (1)
    {
    }
}
