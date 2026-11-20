
#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdlib.h>

/* Task 설정 */
#define NUM_TASK_COUNT  9U
#define NUM_TASK_STACK  1024U

/* UART 및 Binary Semaphore */
UART_HandleTypeDef huart3;
osSemaphoreId myBinarySemHandle;

/* Task별 번호와 Handle */
static uint32_t numArguments[NUM_TASK_COUNT] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9
};

static osThreadId numTaskHandles[NUM_TASK_COUNT] = {0};

/* 함수 선언 */
void SystemClock_Config(void);
static void MX_USART3_UART_Init(void);
void StartNumTask(void const *argument);

/* 메인 함수 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    /* Binary Semaphore 생성: 초기 Count = 1 */
    osSemaphoreDef(myBinarySem);
    myBinarySemHandle = osSemaphoreCreate(
        osSemaphore(myBinarySem), 1
    );

    if (myBinarySemHandle == NULL)
    {
        Error_Handler();
    }

    /* 난수 초기화 */
    srand(1234U);

    /* 하나의 함수로 Task 9개 생성 */
    osThreadDef(
        num_task,
        StartNumTask,
        osPriorityNormal,
        0,
        NUM_TASK_STACK
    );

    for (uint32_t i = 0; i < NUM_TASK_COUNT; i++)
    {
        numTaskHandles[i] = osThreadCreate(
            osThread(num_task),
            &numArguments[i]
        );

        if (numTaskHandles[i] == NULL)
        {
            Error_Handler();
        }
    }

    /* Scheduler 시작 */
    osKernelStart();

    Error_Handler();

    while (1)
    {
    }
}

/* 시스템 클록 설정: 168MHz */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV4;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
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

/* Task 함수: Argument 번호를 UART로 출력 */
void StartNumTask(void const *argument)
{
    const uint32_t taskNumber =
        *(const uint32_t *)argument;

    uint8_t txChar = (uint8_t)('0' + taskNumber);
    uint32_t delayMs;

    for (;;)
    {
        /* Semaphore 획득: UART 접근 보호 */
        if (osSemaphoreWait(
                myBinarySemHandle,
                osWaitForever) >= 0)
        {
            /* 100~500ms 랜덤 지연시간 계산 */
            delayMs = 100U + (uint32_t)(rand() % 401);

            /* UART로 Task 번호 출력 */
            HAL_UART_Transmit(
                &huart3,
                &txChar,
                1U,
                100U
            );

            /* Semaphore 반환 */
            osSemaphoreRelease(myBinarySemHandle);
        }
        else
        {
            delayMs = 100U;
        }

        /* Task를 Blocked 상태로 지연 */
        osDelay(delayMs);
    }
}

/* 오류 발생 시 실행 중지 */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
