
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>
#include <string.h>

/* 0: Inversion (Binary Semaphore), 1: Inheritance (Mutex) */
#define USE_MUTEX  1

/* Task 설정: Stack 단위는 Word */
#define NUM_TASK_COUNT  9U
#define NUM_TASK_STACK  384U

/* UART */
UART_HandleTypeDef huart3;

/* 동기화 객체 */
static SemaphoreHandle_t resourceLock;
static SemaphoreHandle_t uartMutex;

/* Task 번호 */
static uint32_t numArguments[NUM_TASK_COUNT] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9
};

/* Task Handle */
static TaskHandle_t numTaskHandles[NUM_TASK_COUNT] = {0};

/* 함수 선언 */
void SystemClock_Config(void);
static void MX_USART3_UART_Init(void);
static void StartNumTask(void *argument);
static void UART_Log(uint32_t id, const char *msg);
static void BusyWork(uint32_t ms);

/* UART 로그: Task 번호와 메시지 출력 */
static void UART_Log(uint32_t id, const char *msg)
{
    uint8_t digit = (uint8_t)('0' + id);
    uint8_t prefix = '[';
    uint8_t suffix = ']';

    if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE)
    {
        HAL_UART_Transmit(&huart3, &prefix, 1, 100);
        HAL_UART_Transmit(&huart3, &digit, 1, 100);
        HAL_UART_Transmit(&huart3, &suffix, 1, 100);

        HAL_UART_Transmit(
            &huart3,
            (uint8_t *)msg,
            (uint16_t)strlen(msg),
            1000
        );

        xSemaphoreGive(uartMutex);
    }
}

/* CPU 작업: Blocked 상태로 전환하지 않음 */
static void BusyWork(uint32_t ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t duration = pdMS_TO_TICKS(ms);

    while ((xTaskGetTickCount() - start) < duration)
    {
        __NOP();
    }
}

/* Task 1~9가 공유하는 함수 */
static void StartNumTask(void *argument)
{
    uint32_t id = *(uint32_t *)argument;

    /*
     * Priority Inversion / Inheritance
     * Task 1 = Low
     * Task 2 = Medium
     * Task 3 = High
     */

    if (id == 1)
    {
        UART_Log(id, " LOW: Lock request\r\n");

        if (xSemaphoreTake(resourceLock, portMAX_DELAY) == pdTRUE)
        {
            UART_Log(id, " LOW: Lock acquired\r\n");

            /* High Task 깨우기 */
            xTaskNotifyGive(numTaskHandles[2]);

            /*
             * High Task가 Lock을 기다리게 됨.
             * Mutex라면 Low의 우선순위가 상승.
             */

            /* Medium Task 깨우기 */
            xTaskNotifyGive(numTaskHandles[1]);

            UART_Log(id, " LOW: CPU work start\r\n");

            /* 공유 자원 처리: 120ms CPU 작업 */
            BusyWork(120);

            UART_Log(id, " LOW: Lock release\r\n");

            xSemaphoreGive(resourceLock);
        }
    }
    else if (id == 2)
    {
        /* Low Task의 신호를 기다림 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        UART_Log(id, " MEDIUM: CPU work start\r\n");

        /* CPU를 약 300ms 동안 점유 */
        BusyWork(300);

        UART_Log(id, " MEDIUM: CPU work end\r\n");
    }
    else if (id == 3)
    {
        TickType_t start;
        TickType_t elapsed;

        /* Low Task의 신호를 기다림 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        UART_Log(id, " HIGH: Lock request\r\n");

        start = xTaskGetTickCount();

        /* Low가 보유한 공유 자원 요청 */
        if (xSemaphoreTake(resourceLock, portMAX_DELAY) == pdTRUE)
        {
            elapsed = xTaskGetTickCount() - start;

            UART_Log(id, " HIGH: Lock acquired\r\n");

            /* GDB에서 대기시간 확인 */
            volatile TickType_t waitTicks = elapsed;
            (void)waitTicks;

            xSemaphoreGive(resourceLock);
        }
    }


    /*
     * Task 4~9도 기존 NumTask 구조로 실행.
     * Task 1~3은 실험 후 일반 반복 수행.
     */

    if (id >= 4)
    {
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    for (;;)
    {
        UART_Log(id, " NumTask Running\r\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* main */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    /* UART 로그 보호 */
    uartMutex = xSemaphoreCreateMutex();

    if (uartMutex == NULL)
    {
        Error_Handler();
    }

#if USE_MUTEX == 1

    /* Priority Inheritance 지원 */
    resourceLock = xSemaphoreCreateMutex();

#else

    /* Priority Inheritance 없음 */
    resourceLock = xSemaphoreCreateBinary();

    if (resourceLock != NULL)
    {
        xSemaphoreGive(resourceLock);
    }

#endif

    if (resourceLock == NULL)
    {
        Error_Handler();
    }

    /* NumTask 함수 하나로 Task 9개 생성 */
    for (uint32_t i = 0; i < NUM_TASK_COUNT; i++)
    {
        UBaseType_t priority = 1;

        /* Task 1 Low / Task 2 Medium / Task 3 High */
        if (i == 1)
        {
            priority = 2;
        }
        else if (i == 2)
        {
            priority = 3;
        }

        if (xTaskCreate(
                StartNumTask,
                "NumTask",
                NUM_TASK_STACK,
                &numArguments[i],
                priority,
                &numTaskHandles[i]) != pdPASS)
        {
            Error_Handler();
        }
    }

    /* Scheduler 시작 */
    vTaskStartScheduler();

    Error_Handler();

    while (1)
    {
    }
}

/* System Clock: 168MHz */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    osc.OscillatorType =
        RCC_OSCILLATORTYPE_HSE |
        RCC_OSCILLATORTYPE_LSI;

    osc.HSEState = RCC_HSE_BYPASS;
    osc.LSIState = RCC_LSI_ON;

    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 168;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(
            &clk,
            FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USART3: 115200bps */
static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}

/* 오류 처리 */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
