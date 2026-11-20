
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>

/* Task 및 Queue 설정 */
#define NUM_TASK_COUNT    9U
#define TASK_STACK        256U
#define GATE_STACK        256U
#define QUEUE_LENGTH      16U

UART_HandleTypeDef huart3;

/* UART 출력 요청 Queue */
QueueHandle_t uartQueue;

/* Task별 번호 */
static uint32_t numArguments[NUM_TASK_COUNT] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9
};

/* Task Handle */
static TaskHandle_t numTaskHandles[NUM_TASK_COUNT] = {0};
static TaskHandle_t gatekeeperHandle = NULL;

/* 함수 선언 */
void SystemClock_Config(void);
static void MX_USART3_UART_Init(void);

static void NumTask(void *argument);
static void GatekeeperTask(void *argument);

/* 숫자를 Queue에 전송하는 Task */
static void NumTask(void *argument)
{
    uint32_t taskNumber = *(uint32_t *)argument;

    uint8_t txChar = (uint8_t)('0' + taskNumber);

    /* Task마다 다른 전송 주기 */
    TickType_t delayTicks = pdMS_TO_TICKS(
        100U + ((taskNumber * 41U) % 401U)
    );

    for (;;)
    {
        /* Queue에 숫자 전달 */
        xQueueSend(
            uartQueue,
            &txChar,
            portMAX_DELAY
        );

        /* 다음 전송까지 대기 */
        vTaskDelay(delayTicks);
    }
}

/* UART를 독점 관리하는 Gatekeeper Task */
static void GatekeeperTask(void *argument)
{
    uint8_t rxChar;

    (void)argument;

    for (;;)
    {
        /* Queue에서 데이터가 올 때까지 대기 */
        if (xQueueReceive(
                uartQueue,
                &rxChar,
                portMAX_DELAY) == pdTRUE)
        {
            /* Gatekeeper만 USART3에 접근 */
            HAL_UART_Transmit(
                &huart3,
                &rxChar,
                1U,
                100U
            );
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    /* Queue 생성: 1바이트 데이터 16개 저장 */
    uartQueue = xQueueCreate(
        QUEUE_LENGTH,
        sizeof(uint8_t)
    );

    if (uartQueue == NULL)
    {
        Error_Handler();
    }

    /* Gatekeeper Task 생성: 우선순위 2 */
    if (xTaskCreate(
            GatekeeperTask,
            "GATEKEEPER",
            GATE_STACK,
            NULL,
            2,
            &gatekeeperHandle) != pdPASS)
    {
        Error_Handler();
    }

    /* 숫자 송신 Task 9개 생성: 우선순위 1 */
    for (uint32_t i = 0; i < NUM_TASK_COUNT; i++)
    {
        if (xTaskCreate(
                NumTask,
                "NUM_TASK",
                TASK_STACK,
                &numArguments[i],
                1,
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

/* USART3 초기화: 115200bps, 8N1 */
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
