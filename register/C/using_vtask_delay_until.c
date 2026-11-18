
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F429ZI - FreeRTOS Example 5
 * Using vTaskDelayUntil()
 *
 * LD1 PB0:
 *   vTaskDelay(250ms)
 *
 * LD3 PB14:
 *   vTaskDelayUntil(250ms)
 *
 * 차이:
 * vTaskDelay()
 *   호출 시점부터 상대적인 지연
 *
 * vTaskDelayUntil()
 *   이전 Wake Time을 기준으로
 *   일정한 실행 주기 유지
 *
 * HAL 미사용
 * GPIO Register 직접 제어
 *
 * FreeRTOS 구버전 API 기준
 * (일부 신버전은 xTaskDelayUntil 사용)
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
 * 3. 상대 지연 Task
 * ========================================= */

static void Delay_Task(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
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
         * 작업 시간 + 250ms 지연
         *
         * 다음 실행 시점이
         * 이전 작업 수행 시간의 영향을 받음
         */
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/* =========================================
 * 4. 고정 주기 Task
 * ========================================= */

static void Periodic_Task(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    /*
     * 현재 Kernel Tick Count 저장
     *
     * 반복문 밖에서 한 번만 초기화
     */
    TickType_t lastWakeTime = xTaskGetTickCount();

    const TickType_t period = pdMS_TO_TICKS(250);

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
         * 이전 기준 Wake Time + period
         *
         * 일정한 Tick 주기를 유지하도록 대기
         */
        vTaskDelayUntil(
            &lastWakeTime,
            period
        );
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

    /* 상대 지연 Task */
    result1 = xTaskCreate(
        Delay_Task,
        "Delay_Task",
        256,
        NULL,
        1,
        NULL
    );

    /* 고정 주기 Task */
    result2 = xTaskCreate(
        Periodic_Task,
        "Periodic_Task",
        256,
        NULL,
        1,
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
