
/*
 * ============================================================
 * Example 4 — Using the Blocked State to Create a Delay
 * STM32F429ZI / FreeRTOS
 * ============================================================
 *
 * [1. 실습 목적]
 *
 * Example 3에서는 높은 Priority Task가 Busy Waiting을
 * 계속 수행하여 낮은 Priority Task가 실행되지 못했다.
 *
 * Example 4에서는 Busy Waiting을 vTaskDelay()로 변경한다.
 *
 * Task가 작업을 완료한 뒤 Blocked 상태로 들어가면
 * Scheduler는 다른 Ready Task를 실행할 수 있다.
 *
 *
 * [2. Task 구성]
 *
 * Task 2    : Priority 2
 *             문자열 출력 후 vTaskDelay()
 *
 * Task 1    : Priority 1
 *             문자열 출력 후 vTaskDelay()
 *
 * Idle Task : Priority 0
 *             두 Application Task가 모두 Blocked이면 실행
 *
 *
 * [3. Figure 17 — 실행 순서]
 *
 * ① Task 2 Running (Priority 2)
 *          |
 *          v
 *      문자열 출력
 *          |
 *          v
 *      vTaskDelay()
 *          |
 *          v
 *        Blocked
 *
 * ② Task 1 Running (Priority 1)
 *          |
 *          v
 *      문자열 출력
 *          |
 *          v
 *      vTaskDelay()
 *          |
 *          v
 *        Blocked
 *
 * ③ Idle Task Running
 *          |
 *          v
 *      Kernel Tick 경과
 *          |
 *          v
 *      Delay 만료
 *
 * ④ Task 2 / Task 1
 *          |
 *          v
 *        Ready
 *          |
 *          v
 *      Scheduler가 Priority 비교
 *          |
 *          v
 *      높은 Priority Task부터 실행
 *
 *
 * [4. Task State Transition]
 *
 * Running
 *    |
 *    | vTaskDelay()
 *    v
 * Blocked
 *    |
 *    | Delay 만료
 *    v
 * Ready
 *    |
 *    | Scheduler 선택
 *    v
 * Running
 *
 * 주의:
 * Blocked → Ready는 실행 가능한 상태가 되었다는 의미다.
 * 실제 CPU 실행은 Scheduler가 선택한 후 시작된다.
 *
 *
 * [5. Example 3과 Example 4 비교]
 *
 * Example 3
 *   - Busy Waiting 사용
 *   - 높은 Priority Task가 계속 실행 가능
 *   - 낮은 Priority Task에 Starvation 발생 가능
 *   - CPU가 불필요한 반복문을 계속 실행
 *
 * Example 4
 *   - vTaskDelay() 사용
 *   - 높은 Priority Task가 일정 시간 Blocked
 *   - 낮은 Priority Task도 실행 가능
 *   - 두 Task가 모두 Blocked이면 Idle Task 실행
 *   - CPU를 실제 작업이 필요한 Task에 할당 가능
 *
 *
 * [6. Idle Task]
 *
 * 두 Application Task가 모두 Blocked라면
 * Scheduler는 Priority 0의 Idle Task를 선택한다.
 *
 * Idle Task가 실행된다고 CPU가 정지하는 것은 아니다.
 * 별도의 저전력 설정이 없다면 MCU는 계속 동작한다.
 *
 *
 * [7. 핵심]
 *
 * vTaskDelay()는 CPU를 사용하면서 기다리는 함수가 아니다.
 *
 * 현재 Task를 Blocked 상태로 전환하여
 * 다른 Ready Task가 CPU를 사용할 수 있도록 한다.
 *
 * 따라서 Priority 차이를 변경하지 않고도
 * Task 간 CPU 실행 기회를 효율적으로 분배할 수 있다.
 *
 * ============================================================
 */

#include <stdint.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

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
        /* Protect each TXE-check/DR-write pair from task preemption.
         * Individual messages may still interleave when priorities match.
         * This is a small educational UART, not a production logger. */
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

/* Example 4: replace polling/busy delay with vTaskDelay(250ms).
 * Unlike Example 3, both different-priority tasks can execute.
 */
static const char msg1[] = "Task 1 is running\r\n";
static const char msg2[] = "Task 2 is running\r\n";
static void delayed_task(void *arg)
{
    const char *message = (const char *)arg;
    const TickType_t delay = pdMS_TO_TICKS(250);
    for (;;) {
        uart_print(message);
        vTaskDelay(delay);
    }
}
int main(void)
{
    uart_init();
    if (xTaskCreate(delayed_task, "Task1", 256, (void *)msg1, 1, NULL) != pdPASS) for (;;) { }
    if (xTaskCreate(delayed_task, "Task2", 256, (void *)msg2, 2, NULL) != pdPASS) for (;;) { }
    start_scheduler_or_halt();
    return 0;
}
