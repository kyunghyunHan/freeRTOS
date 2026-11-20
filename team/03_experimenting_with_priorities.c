
#include <stdint.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * ============================================================
 * Example 3 — Experimenting with Priorities
 * STM32F429ZI / FreeRTOS
 * ============================================================
 *
 * [1. 실습 목적]
 *
 * Example 1, 2에서는 두 Task의 Priority가 동일했다.
 * Example 3에서는 Priority를 다르게 설정하여
 * Scheduler의 Task 선택 순서를 확인한다.
 *
 * [2. Task 구성]
 *
 * Task 1 : Priority 1
 *          문자열 출력 + Busy Loop
 *
 * Task 2 : Priority 2
 *          문자열 출력 + Busy Loop
 *
 * 두 Task 모두 Blocking API를 사용하지 않는다.
 *
 * [3. 실행 순서]
 *
 * Task 1 : Priority 1, Ready
 * Task 2 : Priority 2, Ready
 *              |
 *              v
 *         Scheduler
 *              |
 *              v
 *         Task 2 Running
 *              |
 *              v
 *          문자열 출력
 *              |
 *              v
 *           Busy Loop
 *              |
 *              v
 *          계속 실행
 *
 * [4. Starvation — 기아 상태]
 *
 * Task 2는 높은 Priority를 가지며,
 * Busy Loop 동안 Blocked 상태로 전환되지 않는다.
 *
 * 따라서 Task 1은 Ready 상태임에도
 * CPU 실행 기회를 얻지 못한다.
 *
 * 이를 Starvation(기아 상태)이라고 한다.
 *
 * [5. 핵심]
 *
 * - Scheduler는 가장 높은 Priority의 Ready Task를 선택한다.
 * - Busy Loop는 Task를 Blocked 상태로 만들지 않는다.
 * - Tick이 발생해도 반드시 Task가 교체되는 것은 아니다.
 * - 낮은 Priority Task는 Ready여도 실행되지 못할 수 있다.
 *
 * ============================================================
 */

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define RCC_AHB1ENR REG32(0x40023830UL)
#define RCC_APB1ENR REG32(0x40023840UL)
#define GPIOD_MODER REG32(0x40020C00UL)
#define GPIOD_AFRH REG32(0x40020C24UL)
#define USART3_SR REG32(0x40004800UL)
#define USART3_DR REG32(0x40004804UL)
#define USART3_BRR REG32(0x40004808UL)
#define USART3_CR1 REG32(0x4000480CUL)

static void uart_init(void)
{
    RCC_AHB1ENR |= (1UL << 3);          /* GPIOD clock */
    RCC_APB1ENR |= (1UL << 18);         /* USART3 clock */
    (void)RCC_APB1ENR;
    GPIOD_MODER = (GPIOD_MODER & ~(3UL << 16)) | (2UL << 16);
    GPIOD_AFRH = (GPIOD_AFRH & ~0xFUL) | 7UL; /* PD8 AF7 */
    USART3_CR1 = 0;
    USART3_BRR = 0x008BUL;             /* 16 MHz / 115200 ~= 138.9 */
    USART3_CR1 = (1UL << 13) | (1UL << 3);  /* UE, TE */
}

static void uart_print(const char *s)
{
    while (*s != '\0')
    {
     
        taskENTER_CRITICAL();
        while ((USART3_SR & (1UL << 7)) == 0U) { }
        USART3_DR = (uint8_t)*s++;
        taskEXIT_CRITICAL();
    }
}

static void start_scheduler_or_halt(void)
{
    vTaskStartScheduler();
    for (;;) { }
}

/*
동일한 Priority로 실행하던 두 Task 중 하나의 Priority를 높였을 때 Scheduler의 동작이 어떻게 달라지는지 확인하는 예제
*/
#define SPIN_COUNT 200000UL
static const char msg1[] = "Task 1 priority 1\r\n";
static const char msg2[] = "Task 2 priority 2\r\n";
static void shared_task(void *arg)
{
    const char *message = (const char *)arg;
    for (;;) {
        uart_print(message);
        for (volatile uint32_t i = 0; i < SPIN_COUNT; ++i) { }
    }
}
int main(void)
{
    uart_init();
    if (xTaskCreate(shared_task, "Task1", 256, (void *)msg1, 1, NULL) != pdPASS) for (;;) { }
    if (xTaskCreate(shared_task, "Task2", 256, (void *)msg2, 2, NULL) != pdPASS) for (;;) { }
    start_scheduler_or_halt();
    return 0;
}
