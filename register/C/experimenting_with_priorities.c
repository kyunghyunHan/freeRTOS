
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F429ZI - FreeRTOS Example 3
 * Experimenting with Priorities
 *
 * LD1 PB0  : Low_Task  (Priority 1)
 * LD3 PB14 : High_Task (Priority 2)
 *
 * HAL 사용하지 않음
 * Busy Waiting 사용
 *
 * 예상:
 * Priority 2 Task만 계속 실행
 * Priority 1 Task는 Ready 상태에서 대기
 *
 * configUSE_PREEMPTION = 1
 * configMAX_PRIORITIES >= 3
 */

/* =========================================
 * 1. Register Address
 * ========================================= */

#define RCC_BASE       0x40023800UL
#define GPIOB_BASE     0x40020400UL

#define RCC_AHB1ENR \
    (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

#define GPIOB_MODER \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))

#define GPIOB_OTYPER \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))

#define GPIOB_PUPDR \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))

#define GPIOB_BSRR \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

/* =========================================
 * 2. GPIO 초기화
 * ========================================= */

static void GPIOB_Init(void)
{
    /* GPIOB Clock Enable */
    RCC_AHB1ENR |= (1UL << 1);
    (void)RCC_AHB1ENR;

    /* PB0 Output */
    GPIOB_MODER &= ~(3UL << 0);
    GPIOB_MODER |=  (1UL << 0);

    /* PB14 Output */
    GPIOB_MODER &= ~(3UL << 28);
    GPIOB_MODER |=  (1UL << 28);

    /* Push-Pull */
    GPIOB_OTYPER &= ~((1UL << 0) | (1UL << 14));

    /* No Pull-up / Pull-down */
    GPIOB_PUPDR &= ~((3UL << 0) | (3UL << 28));

    /* LED OFF */
    GPIOB_BSRR = (1UL << 16) | (1UL << 30);
}

/* =========================================
 * 3. Busy Waiting
 * ========================================= */

static void Busy_Wait(void)
{
    /*
     * CPU가 반복문을 직접 실행
     * Blocked 상태로 전환하지 않음
     */
    for (volatile uint32_t i = 0;
         i < 1000000UL;
         i++)
    {
    }
}

/* =========================================
 * 4. Low Priority Task
 * ========================================= */

static void Low_Task(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
        ledOn ^= 1U;

        if (ledOn != 0U)
        {
            /* PB0 HIGH */
            GPIOB_BSRR = (1UL << 0);
        }
        else
        {
            /* PB0 LOW */
            GPIOB_BSRR = (1UL << 16);
        }

        Busy_Wait();
    }
}

/* =========================================
 * 5. High Priority Task
 * ========================================= */

static void High_Task(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
        ledOn ^= 1U;

        if (ledOn != 0U)
        {
            /* PB14 HIGH */
            GPIOB_BSRR = (1UL << 14);
        }
        else
        {
            /* PB14 LOW */
            GPIOB_BSRR = (1UL << 30);
        }

        /*
         * 높은 Priority의 Task가
         * 계속 실행 가능한 상태를 유지
         */
        Busy_Wait();
    }
}

/* =========================================
 * 6. main()
 * ========================================= */

int main(void)
{
    BaseType_t result1;
    BaseType_t result2;

    GPIOB_Init();

    /* Priority 1 */
    result1 = xTaskCreate(
        Low_Task,
        "Low_Task",
        256,
        NULL,
        1,
        NULL
    );

    /* Priority 2 */
    result2 = xTaskCreate(
        High_Task,
        "High_Task",
        256,
        NULL,
        2,
        NULL
    );

    if ((result1 != pdPASS) ||
        (result2 != pdPASS))
    {
        while (1)
        {
        }
    }

    vTaskStartScheduler();

    while (1)
    {
    }
}
