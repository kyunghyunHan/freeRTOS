
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * STM32F446RE / STM32F429ZI
 *
 * FreeRTOS + Register
 * HAL GPIO 사용하지 않음
 *
 * [STM32F446RE - 주석 처리]
 * B1  : PC13
 * LD2 : PA5 (Green)
 *
 * [STM32F429ZI - 활성화]
 * B1  : PC13
 * LD2 : PB7 (Blue)
 *
 * 버튼 누름 → LED ON
 * 버튼 뗌   → LED OFF
 *
 * FreeRTOS Task에서 10ms마다 버튼 확인
 */

/* =========================================
 * 1. RCC Register
 * ========================================= */

#define RCC_BASE       0x40023800UL
#define RCC_AHB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x30UL))

/* =========================================
 * 2. STM32F446RE - GPIOA (주석 처리)
 * ========================================= */

/*
#define GPIOA_BASE     0x40020000UL
#define GPIOA_MODER    (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_OTYPER   (*(volatile uint32_t *)(GPIOA_BASE + 0x04UL))
#define GPIOA_PUPDR    (*(volatile uint32_t *)(GPIOA_BASE + 0x0CUL))
#define GPIOA_BSRR     (*(volatile uint32_t *)(GPIOA_BASE + 0x18UL))

#define LED_PIN        5U
*/

/* =========================================
 * 3. STM32F429ZI - GPIOB (활성화)
 * ========================================= */

#define GPIOB_BASE     0x40020400UL

#define GPIOB_MODER    (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_OTYPER   (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))
#define GPIOB_PUPDR    (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_BSRR     (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

#define LED_PIN        7U

/* =========================================
 * 4. GPIOC - USER Button (공통)
 * ========================================= */

#define GPIOC_BASE     0x40020800UL

#define GPIOC_MODER    (*(volatile uint32_t *)(GPIOC_BASE + 0x00UL))
#define GPIOC_PUPDR    (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_IDR      (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))

#define BUTTON_PIN     13U

/* =========================================
 * 5. GPIO Initialization
 * ========================================= */

static void GPIO_Init(void)
{
    /* -------------------------------------
     * STM32F446RE - 주석 처리
     * ------------------------------------- */

    /*
    // GPIOA, GPIOC Clock Enable
    RCC_AHB1ENR |= (1UL << 0) | (1UL << 2);
    (void)RCC_AHB1ENR;

    // PA5 Output Mode
    GPIOA_MODER &= ~(3UL << (LED_PIN * 2U));
    GPIOA_MODER |=  (1UL << (LED_PIN * 2U));

    // PA5 Push-Pull
    GPIOA_OTYPER &= ~(1UL << LED_PIN);

    // PA5 No Pull-up / Pull-down
    GPIOA_PUPDR &= ~(3UL << (LED_PIN * 2U));

    // LED OFF
    GPIOA_BSRR = (1UL << (LED_PIN + 16U));
    */

    /* -------------------------------------
     * STM32F429ZI - 활성화
     * ------------------------------------- */

    // GPIOB, GPIOC Clock Enable
    RCC_AHB1ENR |= (1UL << 1) | (1UL << 2);
    (void)RCC_AHB1ENR;

    // PB7 Output Mode (01)
    GPIOB_MODER &= ~(3UL << (LED_PIN * 2U));
    GPIOB_MODER |=  (1UL << (LED_PIN * 2U));

    // PB7 Push-Pull
    GPIOB_OTYPER &= ~(1UL << LED_PIN);

    // PB7 No Pull-up / Pull-down
    GPIOB_PUPDR &= ~(3UL << (LED_PIN * 2U));

    // PB7 LED OFF
    GPIOB_BSRR = (1UL << (LED_PIN + 16U));

    /* -------------------------------------
     * PC13 Button - 두 보드 공통
     * ------------------------------------- */

    // PC13 Input Mode (00)
    GPIOC_MODER &= ~(3UL << (BUTTON_PIN * 2U));

    // PC13 No Internal Pull-up / Pull-down
    // B1 기본 LOW, 누르면 HIGH
    GPIOC_PUPDR &= ~(3UL << (BUTTON_PIN * 2U));
}

/* =========================================
 * 6. FreeRTOS Button Task
 * ========================================= */

static void Button_Task(void *argument)
{
    (void)argument;

    for (;;)
    {
        /* PC13 Button 상태 읽기 */
        if ((GPIOC_IDR & (1UL << BUTTON_PIN)) != 0U)
        {
            /* 버튼 눌림 -> LED ON */

            // STM32F446RE
            // GPIOA_BSRR = (1UL << LED_PIN);

            // STM32F429ZI
            GPIOB_BSRR = (1UL << LED_PIN);
        }
        else
        {
            /* 버튼 뗌 -> LED OFF */

            // STM32F446RE
            // GPIOA_BSRR = (1UL << (LED_PIN + 16U));

            // STM32F429ZI
            GPIOB_BSRR = (1UL << (LED_PIN + 16U));
        }

        /* 10ms 동안 Task Blocked */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* =========================================
 * 7. Main
 * ========================================= */

int main(void)
{
    /* GPIO Register Initialization */
    GPIO_Init();

    /* FreeRTOS Task Creation */
    BaseType_t result = xTaskCreate(
        Button_Task,
        "Button_Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    if (result != pdPASS)
    {
        while (1)
        {
        }
    }

    /* Start FreeRTOS Scheduler */
    vTaskStartScheduler();

    /* Normally never returns */
    while (1)
    {
    }
}
