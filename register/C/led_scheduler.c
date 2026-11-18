
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"


#define RCC_BASE       0x40023800UL
#define RCC_AHB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

/* =========================================
 * 2. GPIOB - LED
 * ========================================= */

#define GPIOB_BASE     0x40020400UL

#define GPIOB_MODER    (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_OTYPER   (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))
#define GPIOB_PUPDR    (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_BSRR     (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

#define LD1_PIN        0U
#define LD2_PIN        7U

/* =========================================
 * 3. GPIOC - Button
 * ========================================= */

#define GPIOC_BASE     0x40020800UL

#define GPIOC_MODER    (*(volatile uint32_t *)(GPIOC_BASE + 0x00UL))
#define GPIOC_PUPDR    (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_IDR      (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))

#define BUTTON_PIN     13U

/* =========================================
 * 4. GPIO Initialization
 * ========================================= */

static void GPIO_Init(void)
{
    /* GPIOB / GPIOC Clock Enable */
    RCC_AHB1ENR |= (1UL << 1) | (1UL << 2);
    (void)RCC_AHB1ENR;

    /* PB0, PB7 Output Mode (01) */
    GPIOB_MODER &=
        ~((3UL << (LD1_PIN * 2U)) |
          (3UL << (LD2_PIN * 2U)));

    GPIOB_MODER |=
         (1UL << (LD1_PIN * 2U)) |
         (1UL << (LD2_PIN * 2U));

    /* PB0 / PB7 Push-Pull */
    GPIOB_OTYPER &=
        ~((1UL << LD1_PIN) |
          (1UL << LD2_PIN));

    /* PB0 / PB7 No Pull-up / Pull-down */
    GPIOB_PUPDR &=
        ~((3UL << (LD1_PIN * 2U)) |
          (3UL << (LD2_PIN * 2U)));

    /* PC13 Input Mode */
    GPIOC_MODER &=
        ~(3UL << (BUTTON_PIN * 2U));

    /* PC13 No Internal Pull-up / Pull-down */
    GPIOC_PUPDR &=
        ~(3UL << (BUTTON_PIN * 2U));

    /* LD1 / LD2 초기 OFF */
    GPIOB_BSRR =
        (1UL << (LD1_PIN + 16U)) |
        (1UL << (LD2_PIN + 16U));
}

/* =========================================
 * 5. Task 1 - Button Task
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
 * 6. Task 2 - Blink Task
 * ========================================= */

static void Blink_Task(void *argument)
{
    (void)argument;

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
 * 7. Main
 * ========================================= */

int main(void)
{
    /* GPIO 초기화 */
    GPIO_Init();

    /* Task 1 생성 */
    BaseType_t result1 = xTaskCreate(
        Button_Task,
        "Button_Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    /* Task 2 생성 */
    BaseType_t result2 = xTaskCreate(
        Blink_Task,
        "Blink_Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    /* Task 생성 실패 검사 */
    if ((result1 != pdPASS) ||
        (result2 != pdPASS))
    {
        while (1)
        {
        }
    }

    /* Scheduler 한 번만 시작 */
    vTaskStartScheduler();

    /* 정상적으로는 도달하지 않음 */
    while (1)
    {
    }
}
