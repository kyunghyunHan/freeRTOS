
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F429ZI - FreeRTOS Example 2
 * Using the Task Parameter
 *
 * Task Function : Common_Task() 하나
 * Task Instance : Task1, Task2 두 개
 *
 * Task1 -> "Task1 Running"
 * Task2 -> "Task2 Running"
 *
 * Priority = 1
 * Busy Waiting 사용
 * vTaskDelay() 사용하지 않음
 *
 * USART3 TX = PD8 (AF7)
 * Baudrate  = 115200
 * PCLK1     = 16MHz
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
 * 2. Task Parameter
 * ========================================= */

/*
 * Task마다 전달할 문자열
 *
 * 문자열 자체를 복사하는 것이 아니라
 * 각각의 주소를 전달한다.
 */
static const char task1Text[] = "Task1 Running\r\n";
static const char task2Text[] = "Task2 Running\r\n";

/* =========================================
 * 3. UART 초기화
 * ========================================= */

static void UART3_Init(void)
{
    RCC_AHB1ENR |= (1UL << 3);
    RCC_APB1ENR |= (1UL << 18);

    (void)RCC_AHB1ENR;

    /* PD8 Alternate Function */
    GPIOD_MODER &= ~(3UL << 16);
    GPIOD_MODER |=  (2UL << 16);

    /* AF7 */
    GPIOD_AFRH &= ~(0xFUL << 0);
    GPIOD_AFRH |=  (7UL << 0);

    USART3_BRR = 0x008B;
    USART3_CR1 = (1UL << 13) | (1UL << 3);
}

/* =========================================
 * 4. UART 출력
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
 * 5. 공통 Task Function
 * ========================================= */

static void Common_Task(void *argument)
{
    /*
     * void* -> const char* 형변환
     *
     * Task1: task1Text 주소
     * Task2: task2Text 주소
     */
    const char *text = (const char *)argument;

    for (;;)
    {
        /* 전달받은 문자열 출력 */
        UART3_SendString(text);

        /*
         * Busy Waiting
         *
         * 두 Task 모두 동일한 코드를 실행
         * 그러나 각각 다른 Parameter를 가짐
         */
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

    UART3_SendString(
        "\r\nExample 2 - Using the Task Parameter\r\n"
    );

    /*
     * Task Instance 1
     *
     * Function = Common_Task
     * Parameter = task1Text
     */
    result1 = xTaskCreate(
        Common_Task,
        "Task1",
        256,
        (void *)task1Text,
        1,
        NULL
    );

    /*
     * Task Instance 2
     *
     * Function = Common_Task
     * Parameter = task2Text
     */
    result2 = xTaskCreate(
        Common_Task,
        "Task2",
        256,
        (void *)task2Text,
        1,
        NULL
    );

    if ((result1 != pdPASS) ||
        (result2 != pdPASS))
    {
        UART3_SendString("Task Creation Failed\r\n");

        while (1)
        {
        }
    }

    vTaskStartScheduler();

    while (1)
    {
    }
}
