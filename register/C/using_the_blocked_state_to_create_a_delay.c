
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F429ZI - FreeRTOS Example 4
 * Using the Blocked State to Create a Delay
 *
 * LD1 PB0  : Task1 / Priority 1
 * LD3 PB14 : Task2 / Priority 2
 *
 * 두 Task 모두 vTaskDelay(250ms) 사용
 *
 * Task 실행 흐름:
 * Running -> Blocked -> Ready -> Running
 *
 * configUSE_PREEMPTION = 1
 * configTICK_RATE_HZ = 1000 권장
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

    /* No Pull */
    GPIOB_PUPDR &= ~((3UL << 0) | (3UL << 28));

    /* LED OFF */
    GPIOB_BSRR = (1UL << 16) | (1UL << 30);
}

/* =========================================
 * 3. Task1 - Priority 1
 * ========================================= */

static void Task1(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
        /* LED 상태 반전 */
        ledOn ^= 1U;

        if (ledOn != 0U)
        {
            GPIOB_BSRR = (1UL << 0);
        }
        else
        {
            GPIOB_BSRR = (1UL << 16);
        }

        /*
         * 250ms 동안 Blocked
         *
         * Task1이 CPU를 점유하지 않음
         * 다른 Ready Task 실행 가능
         */
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/* =========================================
 * 4. Task2 - Priority 2
 * ========================================= */

static void Task2(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
        ledOn ^= 1U;

        if (ledOn != 0U)
        {
            GPIOB_BSRR = (1UL << 14);
        }
        else
        {
            GPIOB_BSRR = (1UL << 30);
        }

        /*
         * 높은 Priority Task도
         * vTaskDelay()를 호출하면 Blocked
         *
         * 그동안 Task1이 실행 가능
         */
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/* =========================================
 * 5. main()
 * ========================================= */

int main(void)
{
    BaseType_t result1;
    BaseType_t result2;

    GPIOB_Init();

    /* Task1 Priority 1 */
    result1 = xTaskCreate(
        Task1,
        "Task1",
        256,
        NULL,
        1,
        NULL
    );

    /* Task2 Priority 2 */
    result2 = xTaskCreate(
        Task2,
        "Task2",
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
