
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#define RCC_BASE 0x40023800UL
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

#define GPIOB_BASE 0x40020400UL

#define GPIOB_MODER (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_OTYPER (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))
#define GPIOB_PUPDR (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_BSRR (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

#define LD1_PIN 0U
#define LD2_PIN 7U
#define LD3_PIN 14U

/* =========================================
 * 3. GPIOC Register
 * ========================================= */

#define GPIOC_BASE 0x40020800UL

#define GPIOC_MODER (*(volatile uint32_t *)(GPIOC_BASE + 0x00UL))
#define GPIOC_PUPDR (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_IDR (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))

#define BUTTON_PIN 13U

static void GPIO_Init(void)
{
    /* GPIOB / GPIOC Clock Enable */
    RCC_AHB1ENR |= (1UL << 1) | (1UL << 2);
    (void)RCC_AHB1ENR;

    /* PB0, PB7, PB14 Output Mode (01) */
    GPIOB_MODER &=
        ~((3UL << (LD1_PIN * 2U)) |
          (3UL << (LD2_PIN * 2U)) |
          (3UL << (LD3_PIN * 2U)));

    GPIOB_MODER |=
        (1UL << (LD1_PIN * 2U)) |
        (1UL << (LD2_PIN * 2U)) |
        (1UL << (LD3_PIN * 2U));

    /* PB0 / PB7 / PB14 Push-Pull */
    GPIOB_OTYPER &=
        ~((1UL << LD1_PIN) |
          (1UL << LD2_PIN) |
          (1UL << LD3_PIN));

    /* PB0 / PB7 / PB14 No Pull-up / Pull-down */
    GPIOB_PUPDR &=
        ~((3UL << (LD1_PIN * 2U)) |
          (3UL << (LD2_PIN * 2U)) |
          (3UL << (LD3_PIN * 2U)));

    /* PC13 Input Mode */
    GPIOC_MODER &=
        ~(3UL << (BUTTON_PIN * 2U));

    /* PC13 No Internal Pull-up / Pull-down */
    GPIOC_PUPDR &=
        ~(3UL << (BUTTON_PIN * 2U));

    /* LD1 / LD2 / LD3 초기 OFF */
    GPIOB_BSRR =
        (1UL << (LD1_PIN + 16U)) |
        (1UL << (LD2_PIN + 16U)) |
        (1UL << (LD3_PIN + 16U));
}

/* =========================================
 * 5. Task1 - Button Task (LD2)
 * ========================================= */

static void Button_Task(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* PC13 버튼 읽기 */
        if ((GPIOC_IDR & (1UL << BUTTON_PIN)) != 0U)
        {
            /* 버튼 눌림 -> LD2 ON */
            GPIOB_BSRR = (1UL << LD2_PIN);
        }
        else
        {
            /* 버튼 뗌 -> LD2 OFF */
            GPIOB_BSRR = (1UL << (LD2_PIN + 16U));
        }

        /* 10ms 동안 Blocked */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* =========================================
 * 6. Task2 - Blink Task (LD1)
 * ========================================= */

static void Blink_Task(void *argument)
{
    (void)argument;
    //-Wall -Wextra 등의 경고 옵션을 사용할 때 unused parameter 경고를 방지
    uint32_t led_on = 0U;

    for (;;)
    {
        /* LD1 상태 변경 */
        led_on ^= 1U;

        if (led_on != 0U)
        {
            /* LD1 ON */
            GPIOB_BSRR = (1UL << LD1_PIN);
        }
        else
        {
            /* LD1 OFF */
            GPIOB_BSRR = (1UL << (LD1_PIN + 16U));
        }

        /* 500ms 동안 Blocked */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* =========================================
 * 7. Task3 - LD3 Task (PB14)
 * ========================================= */

static void LD3_Task(void *argument)
{
    (void)argument;

    uint32_t led_on = 0U;

    for (;;)
    {
        /* LD3 상태 변경 */
        led_on ^= 1U;

        if (led_on != 0U)
        {
            /* LD3 ON */
            GPIOB_BSRR = (1UL << LD3_PIN);
        }
        else
        {
            /* LD3 OFF */
            GPIOB_BSRR = (1UL << (LD3_PIN + 16U));
        }

        /* 1000ms 동안 Blocked */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* =========================================
 * 8. Main
 * ========================================= */

int main(void)
{
    GPIO_Init();

    /* Task 1 - LD2 Button */
    BaseType_t result1 = xTaskCreate(
        Button_Task,
        "Button_Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 1, // 우선순위
        NULL);

    /* Task 2 - LD1 Blink */
    //Native 방식
    //CMIS-> MX
    BaseType_t result2 = xTaskCreate(
        Blink_Task,
        "Blink_Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL);

    /* Task 3 - LD3 Blink */
    // FreeRTOS 내부에서 Scheduler 시작 전에 마지막으로 생성한 동일 우선순위 Task를 pxCurrentTCB로 지정하기 때문
    // TCB_t * volatile pxCurrentTCB = NULL;
    BaseType_t result3 = xTaskCreate(
        LD3_Task,
        "LD3_Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL);

    /* Task 생성 실패 검사 */
    if ((result1 != pdPASS) ||
        (result2 != pdPASS) ||
        (result3 != pdPASS))
    {
        while (1)
        {
        }
    }

    /* FreeRTOS Scheduler 시작 */
    vTaskStartScheduler();

    /* 정상적으로는 도달하지 않음 */
    while (1)
    {
    }
}
/*
vTaskStartScheduler()
역할	새로운 Task 생성	FreeRTOS 스케줄러 시작
매개변수	6개	0개
TCB	생성 및 초기화	이미 생성된 TCB를 활용
Stack	Task Stack 준비	선택된 Task의 실행 문맥 사용
Ready List	Task 등록	Ready Task 중 실행 대상 선택
호출 횟수	Task마다 호출 가능	일반적으로 한
*/