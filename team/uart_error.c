#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * STM32F429ZI - FreeRTOS
 *
 * NumTask 9개
 * USART3 : 115200bps
 *
 * USE_UART_SEMAPHORE = 0
 *   Binary Semaphore 없이 UART 출력
 *   여러 Task의 문자열이 섞이는 현상 확인
 *
 * USE_UART_SEMAPHORE = 1
 *   Binary Semaphore로 UART 출력 보호
 *   문자열 단위로 출력되는지 확인
 */

#define USE_UART_SEMAPHORE  0

#define NUM_TASK_COUNT  9U
#define NUM_TASK_STACK  384U

/* UART */
UART_HandleTypeDef huart3;

/* Binary Semaphore */
static SemaphoreHandle_t uartSemaphore = NULL;

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
static void UART_SendSlow(const char *message);

/*
 * UART 문자열 출력 함수
 *
 * 한 번에 1바이트씩 전송한다.
 * 각 바이트 사이에 1 Tick Delay를 넣는다.
 *
 * Semaphore OFF:
 *   여러 Task의 출력이 섞일 수 있다.
 *
 * Semaphore ON:
 *   한 Task가 문자열 출력 전체를 완료한 뒤
 *   다른 Task가 출력하도록 보호한다.
 */
static void UART_SendSlow(const char *message)
{
#if USE_UART_SEMAPHORE == 1

    /* Binary Semaphore 획득 */
    if (xSemaphoreTake(
            uartSemaphore,
            portMAX_DELAY) != pdTRUE)
    {
        return;
    }

#endif

    while (*message != '\0')
    {
        uint8_t ch = (uint8_t)*message++;

        /* UART 1바이트 전송 */
        HAL_UART_Transmit(
            &huart3,
            &ch,
            1,
            100
        );

        /*
         * Task를 Blocked 상태로 전환하여
         * 다른 Task가 실행될 기회를 제공
         */
        vTaskDelay(1);
    }

#if USE_UART_SEMAPHORE == 1

    /* 문자열 출력 완료 후 Semaphore 반환 */
    xSemaphoreGive(uartSemaphore);

#endif
}

/*
 * NumTask 1~9
 *
 * 모든 Task는 동일한 함수를 실행하고,
 * 전달받은 argument로 번호를 구분한다.
 */
static void StartNumTask(void *argument)
{
    uint32_t id = *(uint32_t *)argument;

    char message[80];

    uint32_t count = 0;

    for (;;)
    {
        count++;

        snprintf(
            message,
            sizeof(message),
            "[%lu] NumTask Running - Count: %lu\r\n",
            (unsigned long)id,
            (unsigned long)count
        );

        /* 공유 UART로 출력 */
        UART_SendSlow(message);

        /* 다음 반복까지 100ms 대기 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* main */
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_USART3_UART_Init();

    /*
     * FreeRTOS Scheduler 시작 전
     * UART 통신이 정상적인지 확인
     */
    const char testMessage[] =
        "\r\nUART TEST 123456789\r\n";

    HAL_UART_Transmit(
        &huart3,
        (uint8_t *)testMessage,
        sizeof(testMessage) - 1U,
        1000
    );

#if USE_UART_SEMAPHORE == 1

    /* Binary Semaphore 생성 */
    uartSemaphore = xSemaphoreCreateBinary();

    if (uartSemaphore == NULL)
    {
        Error_Handler();
    }

    /*
     * Binary Semaphore 초기 Count = 0
     * 첫 Task가 획득할 수 있도록 Count = 1
     */
    if (xSemaphoreGive(uartSemaphore) != pdTRUE)
    {
        Error_Handler();
    }

#endif

    /* NumTask 9개 생성 */
    for (uint32_t i = 0; i < NUM_TASK_COUNT; i++)
    {
        if (xTaskCreate(
                StartNumTask,
                "NumTask",
                NUM_TASK_STACK,
                &numArguments[i],
                1,
                &numTaskHandles[i]) != pdPASS)
        {
            Error_Handler();
        }
    }

    /* FreeRTOS Scheduler 시작 */
    vTaskStartScheduler();

    /* Scheduler 시작에 실패하면 실행 */
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

    /*
     * PLL 설정 수정
     *
     * HSE BYPASS = 8MHz 가정
     * 8 / 4 * 168 / 2 = 168MHz
     */
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