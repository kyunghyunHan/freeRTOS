
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>
#include <string.h>

/* 1: Deadlock / 2: Recursive Mutex */
#define LAB_MODE 2

#define TASK_STACK 256U

UART_HandleTypeDef huart3;

SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;
SemaphoreHandle_t recursiveMutex;

void SystemClock_Config(void);
static void MX_USART3_UART_Init(void);
static void UART_Send(const char *msg);

static void TaskA(void *argument);
static void TaskB(void *argument);
static void RecursiveTask(void *argument);
static void NestedFunction(void);

/* UART 출력 */
static void UART_Send(const char *msg)
{
    HAL_UART_Transmit(
        &huart3,
        (uint8_t *)msg,
        (uint16_t)strlen(msg),
        1000
    );
}

/* Task A: Mutex A -> Mutex B */
static void TaskA(void *argument)
{
    (void)argument;

    UART_Send("[A] Take Mutex A\r\n");

    xSemaphoreTake(mutexA, portMAX_DELAY);

    UART_Send("[A] Mutex A acquired\r\n");

    vTaskDelay(pdMS_TO_TICKS(100));

    UART_Send("[A] Waiting Mutex B\r\n");

    /* Task B가 Mutex B를 소유하면 대기 */
    xSemaphoreTake(mutexB, portMAX_DELAY);

    UART_Send("[A] Mutex B acquired\r\n");

    xSemaphoreGive(mutexB);
    xSemaphoreGive(mutexA);

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Task B: Mutex B -> Mutex A */
static void TaskB(void *argument)
{
    (void)argument;

    UART_Send("[B] Take Mutex B\r\n");

    xSemaphoreTake(mutexB, portMAX_DELAY);

    UART_Send("[B] Mutex B acquired\r\n");

    vTaskDelay(pdMS_TO_TICKS(100));

    UART_Send("[B] Waiting Mutex A\r\n");

    /* Task A가 Mutex A를 소유하면 대기 */
    xSemaphoreTake(mutexA, portMAX_DELAY);

    UART_Send("[B] Mutex A acquired\r\n");

    xSemaphoreGive(mutexA);
    xSemaphoreGive(mutexB);

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* 중첩 함수: 동일한 Recursive Mutex 재획득 */
static void NestedFunction(void)
{
    UART_Send("[NESTED] Take Recursive Mutex\r\n");

    if (xSemaphoreTakeRecursive(
            recursiveMutex,
            portMAX_DELAY) == pdTRUE)
    {
        UART_Send("[NESTED] Acquired (Count=2)\r\n");

        /* 내부 작업 완료 */
        UART_Send("[NESTED] Working\r\n");

        /* 두 번째 획득분 반환 */
        xSemaphoreGiveRecursive(recursiveMutex);

        UART_Send("[NESTED] Give (Count=1)\r\n");
    }
}

/* Recursive Mutex 실습 Task */
static void RecursiveTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        UART_Send("[TASK] First Take\r\n");

        if (xSemaphoreTakeRecursive(
                recursiveMutex,
                portMAX_DELAY) == pdTRUE)
        {
            UART_Send("[TASK] Acquired (Count=1)\r\n");

            /* 이미 Mutex를 보유한 상태에서 재획득 */
            NestedFunction();

            /* 첫 번째 획득분 반환 */
            xSemaphoreGiveRecursive(recursiveMutex);

            UART_Send("[TASK] Give (Count=0)\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

#if LAB_MODE == 1

    UART_Send("\r\nDeadlock Test\r\n");

    mutexA = xSemaphoreCreateMutex();
    mutexB = xSemaphoreCreateMutex();

    if (mutexA == NULL || mutexB == NULL)
    {
        Error_Handler();
    }

    /* 같은 우선순위의 Task A/B 생성 */
    if (xTaskCreate(
            TaskA,
            "TASK_A",
            TASK_STACK,
            NULL,
            2,
            NULL) != pdPASS)
    {
        Error_Handler();
    }

    if (xTaskCreate(
            TaskB,
            "TASK_B",
            TASK_STACK,
            NULL,
            2,
            NULL) != pdPASS)
    {
        Error_Handler();
    }

#elif LAB_MODE == 2

    UART_Send("\r\nRecursive Mutex Test\r\n");

    recursiveMutex = xSemaphoreCreateRecursiveMutex();

    if (recursiveMutex == NULL)
    {
        Error_Handler();
    }

    if (xTaskCreate(
            RecursiveTask,
            "RECURSIVE",
            TASK_STACK,
            NULL,
            2,
            NULL) != pdPASS)
    {
        Error_Handler();
    }

#else
#error "LAB_MODE must be 1 or 2"
#endif

    /* Scheduler 시작 */
    vTaskStartScheduler();

    Error_Handler();

    while (1)
    {
    }
}

/* 시스템 클록: 168MHz */
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

/* USART3 초기화 */
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
