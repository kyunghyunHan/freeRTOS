
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F429ZI - FreeRTOS Example 6
 * Combining Blocking and Non-blocking Tasks
 *
 * Continuous_Task1:
 *   Priority 1
 *   PB0 LED Toggle
 *   Busy Waiting
 *
 * Continuous_Task2:
 *   Priority 1
 *   PB14 LED Toggle
 *   Busy Waiting
 *
 * Periodic_Task:
 *   Priority 2
 *   3ms Period
 *   UART3 출력
 *   vTaskDelayUntil() 사용
 *
 * HAL 미사용
 * GPIO / UART Register 직접 제어
 *
 * Clock:
 * HSI 16MHz
 * APB1 16MHz
 *
 * FreeRTOS:
 * configUSE_PREEMPTION = 1
 * configUSE_TIME_SLICING = 1
 * configTICK_RATE_HZ = 1000
 * configMAX_PRIORITIES >= 3
 */

/* =========================================
 * 1. Register Address
 * ========================================= */

#define RCC_BASE       0x40023800UL
#define GPIOB_BASE     0x40020400UL
#define GPIOD_BASE     0x40020C00UL
#define USART3_BASE    0x40004800UL

/* RCC */
#define RCC_AHB1ENR \
    (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

#define RCC_APB1ENR \
    (*(volatile uint32_t *)(RCC_BASE + 0x40UL))

/* GPIOB */
#define GPIOB_MODER \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))

#define GPIOB_OTYPER \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))

#define GPIOB_PUPDR \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))

#define GPIOB_BSRR \
    (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

/* GPIOD */
#define GPIOD_MODER \
    (*(volatile uint32_t *)(GPIOD_BASE + 0x00UL))

#define GPIOD_AFRH \
    (*(volatile uint32_t *)(GPIOD_BASE + 0x24UL))

/* USART3 */
#define USART3_SR \
    (*(volatile uint32_t *)(USART3_BASE + 0x00UL))

#define USART3_DR \
    (*(volatile uint32_t *)(USART3_BASE + 0x04UL))

#define USART3_BRR \
    (*(volatile uint32_t *)(USART3_BASE + 0x08UL))

#define USART3_CR1 \
    (*(volatile uint32_t *)(USART3_BASE + 0x0CUL))

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
 * 3. UART 초기화
 * ========================================= */

static void UART3_Init(void)
{
    /* GPIOD Clock Enable */
    RCC_AHB1ENR |= (1UL << 3);

    /* USART3 Clock Enable */
    RCC_APB1ENR |= (1UL << 18);

    (void)RCC_AHB1ENR;

    /* PD8 Alternate Function */
    GPIOD_MODER &= ~(3UL << 16);
    GPIOD_MODER |=  (2UL << 16);

    /* PD8 AF7 */
    GPIOD_AFRH &= ~(0xFUL << 0);
    GPIOD_AFRH |=  (7UL << 0);

    /* PCLK1 = 16MHz, Baudrate = 115200 */
    USART3_BRR = 0x008B;

    /* USART Enable + TX Enable */
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
 * 5. Busy Waiting
 * ========================================= */

static void Busy_Wait(void)
{
    for (volatile uint32_t i = 0;
         i < 1000000UL;
         i++)
    {
    }
}

/* =========================================
 * 6. Continuous Task1
 * ========================================= */

static void Continuous_Task1(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
        /* PB0 Toggle */
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
         * Busy Waiting
         *
         * Blocking API 없음
         * 실행 가능한 상태를 계속 유지
         */
        Busy_Wait();
    }
}

/* =========================================
 * 7. Continuous Task2
 * ========================================= */

static void Continuous_Task2(void *argument)
{
    (void)argument;

    uint32_t ledOn = 0U;

    for (;;)
    {
        /* PB14 Toggle */
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
         * Priority 1
         *
         * Continuous_Task1과 같은 Priority
         * Time Slicing으로 CPU 공유 가능
         */
        Busy_Wait();
    }
}

/* =========================================
 * 8. Periodic Task
 * ========================================= */

static void Periodic_Task(void *argument)
{
    (void)argument;

    /*
     * 현재 Tick Count 저장
     */
    TickType_t lastWakeTime = xTaskGetTickCount();

    /*
     * 3ms를 Tick으로 변환
     *
     * configTICK_RATE_HZ = 1000이면
     * 3ms = 3 Ticks
     */
    const TickType_t period = pdMS_TO_TICKS(3);

    for (;;)
    {
        /*
         * Priority 2인 Periodic Task 실행
         *
         * 높은 Priority이므로
         * Ready 상태가 되면
         * 낮은 Priority Task를 선점할 수 있음
         */
        UART3_SendString("P\r\n");

        /*
         * 3ms 주기 기준으로 대기
         *
         * Running -> Blocked
         *
         * Blocked 동안
         * Priority 1 Task들이 실행 가능
         */
        vTaskDelayUntil(
            &lastWakeTime,
            period
        );
    }
}

/* =========================================
 * 9. main()
 * ========================================= */

int main(void)
{
    BaseType_t result1;
    BaseType_t result2;
    BaseType_t result3;

    GPIOB_Init();
    UART3_Init();

    UART3_SendString("\r\n");
    UART3_SendString("FreeRTOS Example 6\r\n");
    UART3_SendString(
        "Combining Blocking and Non-blocking Tasks\r\n"
    );

    /*
     * Continuous Task1
     * Priority = 1
     */
    result1 = xTaskCreate(
        Continuous_Task1,
        "Continuous1",
        256,
        NULL,
        1,
        NULL
    );

    /*
     * Continuous Task2
     * Priority = 1
     */
    result2 = xTaskCreate(
        Continuous_Task2,
        "Continuous2",
        256,
        NULL,
        1,
        NULL
    );

    /*
     * Periodic Task
     * Priority = 2
     */
    result3 = xTaskCreate(
        Periodic_Task,
        "Periodic",
        256,
        NULL,
        2,
        NULL
    );

    /* 생성 성공 확인 */
    if ((result1 != pdPASS) ||
        (result2 != pdPASS) ||
        (result3 != pdPASS))
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
