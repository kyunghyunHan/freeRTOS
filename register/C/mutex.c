
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*
 * =====================================================
 * STM32F429ZI - FreeRTOS Mutex / Priority Inheritance
 * =====================================================
 *
 * HAL 사용 안 함
 *
 * USART3 TX : PD8 (AF7)
 * Baudrate  : 115200
 * Clock     : HSI 16MHz
 *
 * Task H : Priority 3
 * Task M : Priority 2
 * Task L : Priority 1
 *
 * [실행 순서]
 *
 * 1. main()에서 Mutex 생성
 * 2. H / M / L Task 생성
 * 3. Scheduler 시작
 *
 * 4. H 실행 -> 100ms Delay -> Blocked
 * 5. M 실행 -> 120ms Delay -> Blocked
 * 6. L 실행 -> Mutex 획득
 *
 * 7. L이 공유 자원 작업 수행
 *
 * 8. H Delay 만료
 *    -> L 선점
 *    -> Mutex 획득 시도
 *    -> L이 소유 중이므로 H Blocked
 *
 * 9. Priority Inheritance
 *    -> L의 우선순위가 일시적으로 1 -> 3
 *
 * 10. M Delay 만료
 *     -> M Ready
 *     -> L의 현재 우선순위가 3이므로 대기
 *
 * 11. L 작업 완료
 *     -> Mutex Give
 *     -> L 우선순위 복구
 *
 * 12. H가 Mutex 획득 후 실행
 *
 * 13. H가 Mutex 반환
 *
 * 14. M 실행
 *
 * 주의:
 * - FreeRTOS Kernel / Startup / Linker는
 *   기존 프로젝트에서 제공되어야 한다.
 *
 * - configUSE_MUTEXES = 1
 * - configUSE_PREEMPTION = 1
 * - configMAX_PRIORITIES >= 4
 * =====================================================
 */

// =====================================================
// 1. Configuration
// =====================================================

#define UART_BAUDRATE 115200UL
#define UART_PCLK     16000000UL

#define H_PRIORITY    (tskIDLE_PRIORITY + 3)
#define M_PRIORITY    (tskIDLE_PRIORITY + 2)
#define L_PRIORITY    (tskIDLE_PRIORITY + 1)

#define TASK_STACK_SIZE 512U

#define H_START_DELAY_MS 100U
#define M_START_DELAY_MS 120U
#define L_WORK_TIME_MS   300U

// =====================================================
// 2. Global Mutex
// =====================================================

/*
 * Mutex Handle
 *
 * main()에서 생성한다.
 *
 * 최초 생성 시 Mutex 소유자는 없다.
 *
 * Task가 xSemaphoreTake()에 성공하면
 * 해당 Task가 Mutex의 소유자가 된다.
 */
static SemaphoreHandle_t xMutex = NULL;


// =====================================================
// 3. Clock Initialization
// =====================================================

static void Clock_Init(void)
{
    // HSI 16MHz 활성화
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    // SYSCLK = HSI
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

    // FreeRTOS Cortex-M Interrupt Priority 설정
    NVIC_SetPriorityGrouping(3U);
}


// =====================================================
// 4. USART3 Initialization
// =====================================================

static void UART3_Init(void)
{
    // GPIOD Clock Enable
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    // USART3 Clock Enable
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    // PD8 = Alternate Function
    GPIOD->MODER &= ~(3UL << 16);
    GPIOD->MODER |=  (2UL << 16);

    // Push-Pull
    GPIOD->OTYPER &= ~(1UL << 8);

    // PD8 = AF7 (USART3 TX)
    GPIOD->AFR[1] &= ~(0xFUL << 0);
    GPIOD->AFR[1] |=  (7UL << 0);

    // USART 비활성화 후 설정
    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;

    // 115200bps 설정
    USART3->BRR =
        (UART_PCLK + UART_BAUDRATE / 2U)
        / UART_BAUDRATE;

    // USART Enable + TX Enable
    USART3->CR1 =
        USART_CR1_UE |
        USART_CR1_TE;
}


// =====================================================
// 5. UART Output
// =====================================================

static void UART3_SendChar(char c)
{
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
// 6. Task H - High Priority
// =====================================================

static void Task_H(void *argument)
{
    (void)argument;

    /*
     * [1] H는 우선순위가 가장 높다.
     *
     * Scheduler 시작 시 먼저 실행된다.
     *
     * 하지만 100ms Delay를 호출하므로
     * Running -> Blocked가 된다.
     */
    vTaskDelay(pdMS_TO_TICKS(H_START_DELAY_MS));

    /*
     * [2] 100ms 후 Ready로 전환된다.
     *
     * L이 실행 중이라면 H가 선점한다.
     *
     * Mutex 요청 시각 저장.
     */
    TickType_t request_tick = xTaskGetTickCount();

    /*
     * [3] Mutex 획득 시도
     *
     * L이 Mutex를 소유하고 있으므로
     * H는 Mutex를 즉시 획득할 수 없다.
     *
     * H Running -> Blocked
     *
     * FreeRTOS는 Mutex 소유자인 L에
     * Priority Inheritance를 적용한다.
     *
     * L Priority : 1 -> 3
     */
    if (xSemaphoreTake(
            xMutex,
            portMAX_DELAY
        ) == pdTRUE)
    {
        /*
         * [4] 이 위치까지 왔다는 것은
         * Mutex 획득에 성공했다는 뜻.
         *
         * L이 Mutex를 반환한 후
         * H가 실행을 이어간다.
         */

        TickType_t acquired_tick = xTaskGetTickCount();

        UART3_SendString(
            "[H] Mutex acquired!\r\n"
        );

        UART3_SendString(
            "[H] Waited ticks: "
        );

        UART3_SendU32(
            (uint32_t)(acquired_tick - request_tick)
        );

        UART3_SendString("\r\n");

        /*
         * [5] 공유 자원 사용
         *
         * 현재 Mutex 소유자 = H
         */
        UART3_SendString(
            "[H] Using shared resource\r\n"
        );

        /*
         * [6] Mutex 반환
         *
         * 이제 다른 Task도
         * Mutex 획득을 시도할 수 있다.
         */
        UART3_SendString(
            "[H] Mutex release\r\n"
        );

        xSemaphoreGive(xMutex);
    }

    // 실습 작업 완료 후 주기적으로 Blocked
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// =====================================================
// 7. Task M - Medium Priority
// =====================================================

static void Task_M(void *argument)
{
    (void)argument;

    /*
     * [1] M은 최초 120ms Delay
     *
     * Running -> Blocked
     */
    vTaskDelay(pdMS_TO_TICKS(M_START_DELAY_MS));

    /*
     * [2] 120ms 후 Ready 상태가 된다.
     *
     * 그러나 이때 L은 H의 우선순위 3을
     * 상속받아 실행하고 있다.
     *
     * M의 Priority = 2
     * L의 현재 Priority = 3
     *
     * 따라서 M은 L을 선점하지 못한다.
     *
     * L이 Mutex를 반환하고
     * H가 작업을 마친 뒤
     * M이 실행될 수 있다.
     */
    UART3_SendString(
        "[M] Medium priority task running\r\n"
    );

    // 실습 작업 완료
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// =====================================================
// 8. Task L - Low Priority
// =====================================================

static void Task_L(void *argument)
{
    (void)argument;

    /*
     * [1] H와 M은 처음에 Delay 중이다.
     *
     * 따라서 L이 실행된다.
     *
     * L Priority = 1
     */

    /*
     * [2] Mutex 획득
     *
     * 아직 누구도 Mutex를 소유하지 않는다.
     *
     * 따라서 L의 Take가 성공한다.
     *
     * Mutex Owner = L
     */
    if (xSemaphoreTake(
            xMutex,
            portMAX_DELAY
        ) == pdTRUE)
    {
        UART3_SendString(
            "\r\n[L] Mutex acquired!\r\n"
        );

        UART3_SendString(
            "[L] Start long operation\r\n"
        );

        /*
         * [3] 공유 자원 사용 시작
         *
         * 약 300ms 동안 CPU 작업을 수행한다.
         *
         * 여기서는 Priority Inheritance를
         * 관찰하기 위해 의도적으로
         * Delay 없이 실행하는 예제다.
         *
         * 일반적인 실무 코드에서
         * Mutex를 이렇게 오래 잡는 것은
         * 권장되지 않는다.
         */
        TickType_t start = xTaskGetTickCount();

        while ((TickType_t)(
                    xTaskGetTickCount() - start
               ) < pdMS_TO_TICKS(L_WORK_TIME_MS))
        {
            __NOP();
        }

        /*
         * [4] L이 작업하는 동안 발생한 사건
         *
         * 약 100ms:
         *
         * H가 Ready가 되어 L 선점
         * H가 Mutex Take 시도
         * H Blocked
         *
         * L이 우선순위 3을 상속받는다.
         *
         * 약 120ms:
         *
         * M이 Ready가 된다.
         * 하지만 L의 현재 우선순위는 3이다.
         *
         * M은 실행되지 못한다.
         */

        UART3_SendString(
            "[L] Long operation finished\r\n"
        );

        /*
         * [5] Mutex 반환 직전
         */
        UART3_SendString(
            "[L] Mutex release\r\n"
        );

        /*
         * [6] Mutex 반환
         *
         * Mutex를 기다리던 H가
         * Ready 상태로 바뀐다.
         *
         * L의 상속 우선순위가 복구된다.
         *
         * H Priority = 3
         * L Priority = 1
         *
         * Scheduler가 H를 선택한다.
         */
        xSemaphoreGive(xMutex);
    }

    // 실습 작업 완료
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// =====================================================
// 9. FreeRTOS Object Initialization
// =====================================================

static void RTOS_Objects_Init(void)
{
    BaseType_t result;

    /*
     * [1] Mutex 생성
     *
     * 최초 소유자 없음.
     *
     * FreeRTOS Mutex에는
     * Priority Inheritance 기능이 있다.
     */
    xMutex = xSemaphoreCreateMutex();

    configASSERT(xMutex != NULL);

    if (xMutex == NULL)
    {
        for (;;)
        {
        }
    }

    /*
     * [2] Task H 생성
     *
     * Priority = 3
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

    /*
     * [3] Task M 생성
     *
     * Priority = 2
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

    /*
     * [4] Task L 생성
     *
     * Priority = 1
     */
    result = xTaskCreate(
        Task_L,
        "TASK_L",
        TASK_STACK_SIZE,
        NULL,
        L_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);
}


// =====================================================
// 10. Main
// =====================================================

int main(void)
{
    /*
     * [1] Clock 초기화
     */
    Clock_Init();

    /*
     * [2] USART3 초기화
     */
    UART3_Init();

    /*
     * [3] FreeRTOS Mutex와 Task 생성
     */
    RTOS_Objects_Init();

    /*
     * [4] Scheduler 시작
     */
    vTaskStartScheduler();

    for (;;)
    {
    }
}
