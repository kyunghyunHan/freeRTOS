
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * =====================================================
 * STM32F429ZI - FreeRTOS Preemption LED Example
 * =====================================================
 *
 * HAL 사용 안 함
 *
 * LD1 : PB0 (Active-High)
 * LD2 : PB7 (Active-High)
 *
 * H Task : Priority 3
 *   LD1 ON
 *   1초 동안 CPU 작업
 *   LD1 OFF
 *   1초 Delay (Blocked)
 *   반복
 *
 * M Task : Priority 2
 *   LD2 ON
 *   10초 동안 CPU 작업 (경과 시간 기준)
 *   LD2 OFF
 *   1초 Delay
 *   반복
 *
 * 핵심:
 *
 * H가 Running이면 M은 Ready로 대기
 * H가 Blocked 되면 M이 Running
 * H의 Delay가 끝나면 M을 선점
 *
 * FreeRTOSConfig.h:
 *
 * configUSE_PREEMPTION = 1
 * configMAX_PRIORITIES >= 4
 *
 * 기존 FreeRTOS Kernel, Startup,
 * Linker Script가 필요하다.
 * =====================================================
 */

// =====================================================
// 1. Configuration
// =====================================================

#define LD1_PIN          0U
#define LD2_PIN          7U

#define H_PRIORITY       (tskIDLE_PRIORITY + 3)
#define M_PRIORITY       (tskIDLE_PRIORITY + 2)

#define TASK_STACK_SIZE  256U

#define H_WORK_MS        1000U
#define H_DELAY_MS       1000U

#define M_WORK_MS        10000U
#define M_DELAY_MS       1000U

// =====================================================
// 2. Clock Initialization
// =====================================================

static void Clock_Init(void)
{
    // HSI 16MHz 활성화
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    // SYSCLK = HSI
    // AHB, APB1, APB2 = 16MHz
    RCC->CFGR &= ~(
        RCC_CFGR_SW |
        RCC_CFGR_HPRE |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );

    while ((RCC->CFGR & RCC_CFGR_SWS)
            != RCC_CFGR_SWS_HSI)
    {
    }

    SystemCoreClockUpdate();

    configASSERT(SystemCoreClock == 16000000UL);
}

// =====================================================
// 3. GPIO Initialization
// =====================================================

static void GPIO_Init(void)
{
    // GPIOB Clock Enable
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    (void)RCC->AHB1ENR;

    /*
     * PB0 = LD1
     * PB7 = LD2
     *
     * MODER = 01 (General Purpose Output)
     */

    GPIOB->MODER &= ~(
        (3UL << (LD1_PIN * 2U)) |
        (3UL << (LD2_PIN * 2U))
    );

    GPIOB->MODER |= (
        (1UL << (LD1_PIN * 2U)) |
        (1UL << (LD2_PIN * 2U))
    );

    // Push-Pull
    GPIOB->OTYPER &= ~(
        (1UL << LD1_PIN) |
        (1UL << LD2_PIN)
    );

    // 초기 LED 모두 OFF
    GPIOB->BSRR =
        (1UL << (LD1_PIN + 16U)) |
        (1UL << (LD2_PIN + 16U));
}

// =====================================================
// 4. LED Functions
// =====================================================

static void LD1_ON(void)
{
    GPIOB->BSRR = (1UL << LD1_PIN);
}

static void LD1_OFF(void)
{
    GPIOB->BSRR = (1UL << (LD1_PIN + 16U));
}

static void LD2_ON(void)
{
    GPIOB->BSRR = (1UL << LD2_PIN);
}

static void LD2_OFF(void)
{
    GPIOB->BSRR = (1UL << (LD2_PIN + 16U));
}

// =====================================================
// 5. CPU Work Function
// =====================================================

/*
 * CPU 작업을 흉내 내는 함수.
 *
 * vTaskDelay()를 사용하지 않는다.
 *
 * 실행하는 Task는 Tick을 계속 확인하며
 * Running 상태로 CPU를 사용한다.
 *
 * 높은 우선순위 Task가 Ready가 되면
 * 이 함수 실행 중에도 선점당할 수 있다.
 *
 * 참고:
 * Tick 기준의 경과 시간을 측정하므로
 * 선점된 시간도 duration에 포함된다.
 */

static void CPU_Work(uint32_t duration_ms)
{
    TickType_t start = xTaskGetTickCount();

    TickType_t duration =
        pdMS_TO_TICKS(duration_ms);

    while ((TickType_t)(
                xTaskGetTickCount() - start
           ) < duration)
    {
        __NOP();
    }
}

// =====================================================
// 6. Task H - High Priority
// =====================================================

static void Task_H(void *argument)
{
    (void)argument;

    /*
     * H Priority = 3
     *
     * 가장 높은 우선순위이므로
     * Scheduler 시작 후 H가 먼저 실행된다.
     */

    for (;;)
    {
        /*
         * [1] LD1 ON
         *
         * H = Running
         * M = Ready
         */
        LD1_ON();

        /*
         * [2] 1초 CPU 작업
         *
         * Delay 없이 1초 동안 CPU 작업.
         *
         * 이 동안 M은 H보다 우선순위가 낮아서
         * 일반적으로 실행하지 못한다.
         */
        CPU_Work(H_WORK_MS);

        /*
         * [3] LD1 OFF
         */
        LD1_OFF();

        /*
         * [4] H가 1초 동안 Blocked
         *
         * H : Running -> Blocked
         *
         * 이제 M이 Ready 상태라면
         * Scheduler가 M을 선택한다.
         */
        vTaskDelay(pdMS_TO_TICKS(H_DELAY_MS));

        /*
         * [5] Delay 만료
         *
         * H : Blocked -> Ready
         *
         * M이 실행 중이어도
         * H의 우선순위가 더 높으므로
         * M을 선점한다.
         *
         * H : Ready -> Running
         * M : Running -> Ready
         *
         * 이후 반복문 처음으로 이동.
         */
    }
}

// =====================================================
// 7. Task M - Medium Priority
// =====================================================

static void Task_M(void *argument)
{
    (void)argument;

    /*
     * M Priority = 2
     *
     * H가 Delay를 호출하면 M이 실행된다.
     */

    for (;;)
    {
        /*
         * [1] LD2 ON
         *
         * M = Running
         */
        LD2_ON();

        /*
         * [2] 10초 CPU 작업
         *
         * 중요한 점:
         *
         * M은 10초 경과를 기다리는 동안
         * 별도의 Delay를 호출하지 않는다.
         *
         * 하지만 H가 1초 Delay를 마치고
         * Ready 상태가 되면 M이 선점된다.
         *
         * 예:
         *
         * M Running
         *     |
         *     v
         * H Delay 종료
         *     |
         *     v
         * M Running -> Ready
         * H Ready   -> Running
         *     |
         *     v
         * H가 1초 CPU 작업
         *     |
         *     v
         * H가 다시 Delay
         *     |
         *     v
         * M Ready -> Running
         *
         * M은 CPU_Work()의 중단 지점부터
         * 실행을 이어간다.
         */
        CPU_Work(M_WORK_MS);

        /*
         * [3] 10초 경과
         *
         * 중간에 여러 번 선점당했더라도
         * CPU_Work()가 반환하면 실행된다.
         */
        LD2_OFF();

        /*
         * [4] M도 1초 Delay
         */
        vTaskDelay(pdMS_TO_TICKS(M_DELAY_MS));
    }
}

// =====================================================
// 8. Task Creation
// =====================================================

static void RTOS_Objects_Init(void)
{
    BaseType_t result;

    /*
     * [1] H 생성
     *
     * Priority 3
     */
    result = xTaskCreate(
        Task_H,
        "TASK_H",
        TASK_STACK_SIZE,
        NULL,
        H_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);

    if (result != pdPASS)
    {
        for (;;)
        {
        }
    }

    /*
     * [2] M 생성
     *
     * Priority 2
     */
    result = xTaskCreate(
        Task_M,
        "TASK_M",
        TASK_STACK_SIZE,
        NULL,
        M_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);

    if (result != pdPASS)
    {
        for (;;)
        {
        }
    }
}

// =====================================================
// 9. Main
// =====================================================

int main(void)
{
    // 1. Clock 초기화
    Clock_Init();

    // 2. GPIO 초기화
    GPIO_Init();

    // 3. Task 생성
    RTOS_Objects_Init();

    // 4. Scheduler 시작
    vTaskStartScheduler();

    for (;;)
    {
    }
}
