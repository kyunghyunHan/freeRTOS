
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * =====================================================
 * STM32F429ZI - FreeRTOS Preemption Example
 * =====================================================
 *
 * 목적:
 *   높은 우선순위 Task H가
 *   실행 중인 Task M을 선점하는 상황 확인
 *
 * HAL 사용 안 함
 *
 * USART3 TX : PD8 (AF7)
 * Baudrate  : 115200, 8N1
 * Clock     : HSI 16MHz
 *
 * Task H:
 *   Priority 3
 *   시작 후 1000ms Delay
 *   깨어나면 M을 선점하고 UART 출력
 *
 * Task M:
 *   Priority 2
 *   약 3000ms 동안 CPU 작업 수행
 *   작업 중 H에 의해 선점됨
 *   H가 Blocked 되면 중단된 지점부터 재개
 *
 * FreeRTOS 설정:
 *   configUSE_PREEMPTION = 1
 *   configMAX_PRIORITIES >= 4
 *
 * 실행 조건:
 *   FreeRTOS Kernel / Startup / Linker 연결
 *   다른 Task가 이 실습에 간섭하지 않는 구성
 * =====================================================
 */

#define UART_PCLK       16000000UL
#define UART_BAUDRATE   115200UL

#define H_PRIORITY      (tskIDLE_PRIORITY + 3)
#define M_PRIORITY      (tskIDLE_PRIORITY + 2)

#define STACK_SIZE      512U

// =====================================================
// 1. Clock Initialization
// =====================================================

static void Clock_Init(void)
{
    // HSI 16MHz 활성화
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    // System Clock = HSI
    // AHB / APB1 / APB2 Prescaler = 1
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

    NVIC_SetPriorityGrouping(3U);
}

// =====================================================
// 2. USART3 Initialization
// =====================================================

static void UART3_Init(void)
{
    // GPIOD Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    // USART3 Clock
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    // PD8 Alternate Function
    GPIOD->MODER &= ~(3UL << 16);
    GPIOD->MODER |=  (2UL << 16);

    // Push-Pull
    GPIOD->OTYPER &= ~(1UL << 8);

    // PD8 = AF7
    GPIOD->AFR[1] &= ~(0xFUL << 0);
    GPIOD->AFR[1] |=  (7UL << 0);

    // USART Disable
    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;

    // Baudrate
    USART3->BRR =
        (UART_PCLK + UART_BAUDRATE / 2U)
        / UART_BAUDRATE;

    // USART Enable / TX Enable
    USART3->CR1 =
        USART_CR1_UE |
        USART_CR1_TE;
}

// =====================================================
// 3. UART TX Functions
// =====================================================

static void UART3_SendChar(char c)
{
    // TX Data Register Empty 확인
    while ((USART3->SR & USART_SR_TXE) == 0U)
    {
    }

    USART3->DR = (uint8_t)c;
}

static void UART3_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART3_SendChar(*str++);
    }
}

static void UART3_SendU32(uint32_t value)
{
    char buffer[11];
    uint32_t index = 0U;

    if (value == 0U)
    {
        UART3_SendChar('0');
        return;
    }

    while (value > 0U)
    {
        buffer[index++] =
            (char)('0' + (value % 10U));

        value /= 10U;
    }

    while (index > 0U)
    {
        UART3_SendChar(buffer[--index]);
    }
}

// =====================================================
// 4. Task H - High Priority
// =====================================================

static void Task_H(void *argument)
{
    (void)argument;

    /*
     * [1] H Priority = 3
     *
     * Scheduler 시작 직후 H가 먼저 실행된다.
     */

    /*
     * [2] 1000ms Delay
     *
     * H:
     * Running -> Blocked
     *
     * H가 Blocked 상태가 되었으므로
     * Scheduler는 M을 실행한다.
     */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /*
     * [3] 1000ms 경과
     *
     * FreeRTOS Tick 처리로
     *
     * H:
     * Blocked -> Ready
     *
     * H Priority = 3
     * M Priority = 2
     *
     * H가 M을 선점한다.
     *
     * M:
     * Running -> Ready
     *
     * H:
     * Ready -> Running
     */

    UART3_SendString(
        "\r\n"
        "================================\r\n"
        "[H] PREEMPT! HIGH PRIORITY RUNNING\r\n"
        "[H] M was interrupted!\r\n"
        "[H] Tick: "
    );

    UART3_SendU32(xTaskGetTickCount());

    UART3_SendString(
        "\r\n"
        "[H] Work finished\r\n"
        "================================\r\n"
    );

    /*
     * [4] H 작업 완료
     *
     * H가 다시 Blocked 상태가 된다.
     *
     * Scheduler는 M이 Ready 상태라면
     * M을 다시 실행할 수 있다.
     *
     * M은 처음부터 실행하지 않고
     * 선점당했던 지점부터 이어서 실행한다.
     */
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =====================================================
// 5. Task M - Medium Priority
// =====================================================

static void Task_M(void *argument)
{
    (void)argument;

    /*
     * [1] H가 Delay를 호출했으므로
     * M이 실행된다.
     *
     * M Priority = 2
     */

    UART3_SendString(
        "\r\n"
        "[M] START - Long operation\r\n"
        "[M] Tick: "
    );

    UART3_SendU32(xTaskGetTickCount());

    UART3_SendString("\r\n");

    /*
     * [2] 약 3000ms 동안 CPU 작업
     *
     * 현재 코드에는 vTaskDelay()가 없다.
     *
     * M은 Tick을 계속 검사하며
     * CPU를 사용한다.
     *
     * 단, 우선순위가 더 높은 H가
     * Ready 상태가 되면 M은 선점된다.
     */
    TickType_t start_tick = xTaskGetTickCount();

    while ((TickType_t)(
               xTaskGetTickCount() - start_tick
           ) < pdMS_TO_TICKS(3000))
    {
        /*
         * CPU 작업을 수행하는 부분.
         *
         * H가 1000ms 후 Ready가 되면
         * 바로 여기에서 M이 선점될 수 있다.
         *
         * Context Switching:
         *
         * M의 CPU Context 저장
         * H의 CPU Context 복원
         *
         * H 실행
         *
         * 이후 M이 선택되면
         * 저장된 Context를 복원하여
         * 이 반복문 실행을 이어간다.
         */
        __NOP();
    }

    /*
     * [3] 3000ms 경과
     *
     * M의 CPU 작업이 완료됐다.
     *
     * 중간에 H가 선점했지만
     * M은 작업을 계속 이어서 수행했다.
     */

    UART3_SendString(
        "\r\n"
        "[M] END - Long operation\r\n"
        "[M] Tick: "
    );

    UART3_SendU32(xTaskGetTickCount());

    UART3_SendString(
        "\r\n"
        "[M] Resumed after H preemption\r\n"
    );

    /*
     * [4] 작업 종료
     *
     * 실습 이후 M도 Blocked 상태 유지
     */
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =====================================================
// 6. FreeRTOS Task Creation
// =====================================================

static void RTOS_Objects_Init(void)
{
    BaseType_t result;

    /*
     * [1] H Task 생성
     *
     * 생성 직후 Ready 상태.
     */
    result = xTaskCreate(
        Task_H,
        "TASK_H",
        STACK_SIZE,
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
     * [2] M Task 생성
     *
     * 생성 직후 Ready 상태.
     */
    result = xTaskCreate(
        Task_M,
        "TASK_M",
        STACK_SIZE,
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
// 7. Main
// =====================================================

int main(void)
{
    /*
     * [1] HSI Clock 설정
     */
    Clock_Init();

    /*
     * [2] USART3 초기화
     */
    UART3_Init();

    /*
     * [3] Task H / Task M 생성
     */
    RTOS_Objects_Init();

    /*
     * [4] Scheduler 시작
     *
     * H와 M이 모두 Ready이므로
     * 높은 우선순위인 H부터 실행.
     */
    vTaskStartScheduler();

    for (;;)
    {
    }
}
