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

static SemaphoreHandle_t recursiveMutex;

/* 실제 응용과 유사하게 중첩 호출 내부에서 재획득 */
static void NestedFunction(uint32_t id)
{
    if (xSemaphoreTakeRecursive(recursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        UART_Log(id, " nested Take: depth 2\r\n");
        /* 공유 자원을 사용하는 내부 함수 */
        xSemaphoreGiveRecursive(recursiveMutex);
        UART_Log(id, " nested Give: depth 1\r\n");
    }
}

static void StartNumTask(void *argument)
{
    const uint32_t id = *(const uint32_t *)argument;

    if (id == 1U)
    {
        for (;;)
        {
            if (xSemaphoreTakeRecursive(recursiveMutex,
                                        portMAX_DELAY) == pdTRUE)
            {
                UART_Log(id, " first Take: depth 1\r\n");
                NestedFunction(id);
                xSemaphoreGiveRecursive(recursiveMutex);
                UART_Log(id, " final Give: depth 0\r\n");
            }
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }
    else
    {
        BackgroundNumTask(id);
    }
}
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    uartMutex = xSemaphoreCreateMutex();
    if (uartMutex == NULL) Error_Handler();

    recursiveMutex = xSemaphoreCreateRecursiveMutex();
    if (recursiveMutex == NULL) Error_Handler();

    /* 같은 StartNumTask() 함수를 9번 생성 */
    for (uint32_t i = 0; i < NUM_TASK_COUNT; ++i)
    {
        UBaseType_t priority = 1U;
        
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
