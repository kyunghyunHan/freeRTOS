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
#define LD3_PIN 14U

static void GPIO_Init(void)

{

    RCC_AHB1ENR |= (1UL << 1);

    (void)RCC_AHB1ENR;

    GPIOB_MODER &=

        ~((3UL << (LD1_PIN * 2U)) |

          (3UL << (LD3_PIN * 2U)));

    GPIOB_MODER |=

        (1UL << (LD1_PIN * 2U)) |

        (1UL << (LD3_PIN * 2U));

    /* Push-Pull */

    GPIOB_OTYPER &=

        ~((1UL << LD1_PIN) |

          (1UL << LD3_PIN));

    /* No Pull-up / Pull-down */

    GPIOB_PUPDR &=

        ~((3UL << (LD1_PIN * 2U)) |

          (3UL << (LD3_PIN * 2U)));

    /* LD1 / LD3 초기 OFF */

    GPIOB_BSRR =

        (1UL << (LD1_PIN + 16U)) |

        (1UL << (LD3_PIN + 16U));
}

// Task Function
static void Blink_500ms_Task(void *argument)

{

    (void)argument;

    uint32_t led_on = 0U;

    for (;;)

    {

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

 * 5. Task 2 - LD3 (1000ms)

 * ========================================= */

static void Blink_1000ms_Task(void *argument)

{

    (void)argument;

    uint32_t led_on = 0U;

    for (;;)

    {

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

 * 6. Main

 * ========================================= */

int main(void)

{

    GPIO_Init();

    // Task Instance
    BaseType_t result1 = xTaskCreate(

        Blink_500ms_Task,

        "Blink_500ms",

        256,

        NULL,

        tskIDLE_PRIORITY + 1,

        NULL

    );
    BaseType_t result2 = xTaskCreate(

        Blink_500ms_Task,

        "Blink_500ms",

        256,

        NULL,

        tskIDLE_PRIORITY + 1,

        NULL

    );

    BaseType_t result3 = xTaskCreate(

        Blink_500ms_Task,

        "Blink_500ms",

        256,

        NULL,

        tskIDLE_PRIORITY + 1,

        NULL

    );

    /* Task 2 - LD3 1000ms */

    BaseType_t result2 = xTaskCreate(

        Blink_1000ms_Task,

        "Blink_1000ms",

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

    /* FreeRTOS Scheduler 시작 */

    vTaskStartScheduler();

    /* 정상적으로는 도달하지 않음 */

    while (1)

    {
    }
}
