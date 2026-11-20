#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>
#include <string.h>

/* NumTask 함수 1개로 Task 9개 생성 */
#define NUM_TASK_COUNT 9U
#define NUM_TASK_STACK 256U /* FreeRTOS native API: 32-bit Word */

UART_HandleTypeDef huart3;
static SemaphoreHandle_t uartMutex;
static uint32_t numArguments[NUM_TASK_COUNT] = {1,2,3,4,5,6,7,8,9};
static TaskHandle_t numTaskHandles[NUM_TASK_COUNT] = {0};

void SystemClock_Config(void);
static void MX_USART3_UART_Init(void);
static void StartNumTask(void *argument);

/* UART 로그도 서로 섞이지 않도록 별도 Mutex 사용 */
static void UART_Log(uint32_t id, const char *msg)
{
    uint8_t prefix[3] = {'[', (uint8_t)('0' + id), ']'};
    if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE)
    {
        HAL_UART_Transmit(&huart3, prefix, sizeof(prefix), 100U);
        HAL_UART_Transmit(&huart3, (uint8_t *)msg,
                          (uint16_t)strlen(msg), 1000U);
        xSemaphoreGive(uartMutex);
    }
}

/* 실험에 직접 참여하지 않는 Task 3~9도 NumTask로 유지 */
static void BackgroundNumTask(uint32_t id)
{
    vTaskDelay(pdMS_TO_TICKS(1000U + id * 100U));
    for (;;)
    {
        UART_Log(id, " NumTask running\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

/* 0: Binary Semaphore (상속 없음), 1: Mutex (상속 있음) */
#define USE_MUTEX 0
static SemaphoreHandle_t resourceLock;
volatile TickType_t highWaitTicks = 0;

static void BusyWork(uint32_t milliseconds)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t duration = pdMS_TO_TICKS(milliseconds);
    while ((xTaskGetTickCount() - start) < duration)
        __NOP();
}

static void StartNumTask(void *argument)
{
    const uint32_t id = *(const uint32_t *)argument;

    if (id == 1U) /* Low: priority 1 */
    {
        xSemaphoreTake(resourceLock, portMAX_DELAY);
        UART_Log(id, " LOW acquired resource\r\n");

        /* High Task를 먼저 깨운다. High는 Lock을 기다리게 된다. */
        xTaskNotifyGive(numTaskHandles[2]);

        /* Mutex 버전이면 Low가 High의 priority를 상속한다. */
        UART_Log(id, " LOW working while holding lock\r\n");
        BusyWork(150U);

        xSemaphoreGive(resourceLock);
        UART_Log(id, " LOW released resource\r\n");
    }
    else if (id == 2U) /* Medium: priority 2 */
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        UART_Log(id, " MEDIUM CPU work start\r\n");
        BusyWork(300U);
        UART_Log(id, " MEDIUM CPU work end\r\n");
    }
    else if (id == 3U) /* High: priority 3 */
    {
        TickType_t start;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        UART_Log(id, " HIGH requests resource\r\n");
        /* Medium은 High가 resourceLock을 기다린 후에 실행한다. */
        xTaskNotifyGive(numTaskHandles[1]);
        start = xTaskGetTickCount();
        xSemaphoreTake(resourceLock, portMAX_DELAY);
        highWaitTicks = xTaskGetTickCount() - start;
        UART_Log(id, " HIGH acquired resource\r\n");
        xSemaphoreGive(resourceLock);
    }
    else
    {
        BackgroundNumTask(id);
    }

    for (;;) vTaskDelay(pdMS_TO_TICKS(1000U));
}
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    uartMutex = xSemaphoreCreateMutex();
    if (uartMutex == NULL) Error_Handler();

#if USE_MUTEX
    resourceLock = xSemaphoreCreateMutex();
#else
    resourceLock = xSemaphoreCreateBinary();
    if (resourceLock != NULL) xSemaphoreGive(resourceLock); /* 토큰 1개 */
#endif
    if (resourceLock == NULL) Error_Handler();

    /* 같은 StartNumTask() 함수를 9번 생성 */
    for (uint32_t i = 0; i < NUM_TASK_COUNT; ++i)
    {
        UBaseType_t priority = 1U;
        if (i == 1U) priority = 2U;  /* NumTask 2 */
        if (i == 2U) priority = 3U;  /* NumTask 3 */
        if (xTaskCreate(StartNumTask, "NumTask", NUM_TASK_STACK,
                        &numArguments[i], priority,
                        &numTaskHandles[i]) != pdPASS)
            Error_Handler();
    }

    vTaskStartScheduler();
    Error_Handler();
    while (1) { }
}

/* STM32F429ZI: 기존 Nucleo HSE bypass / PLL 168MHz 설정 유지 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    osc.HSEState = RCC_HSE_BYPASS;
    osc.LSIState = RCC_LSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 168;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

/* USART3: PD8(TX), PD9(RX) AF7 은 기존 HAL MSP 초기화 이용 */
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
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
