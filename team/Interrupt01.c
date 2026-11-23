
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>

/*
 * STM32F429ZI + FreeRTOS
 *
 * B1    : PC13 (EXTI Rising Edge)
 * LD2   : PB7  (LED Toggle)
 * USART3: PD8(TX), PD9(RX)
 * Baud  : 115200, 8N1
 *
 * 동작:
 *
 * 1. Binary Semaphore 생성 (초기 0)
 * 2. KeyTask 생성
 * 3. Scheduler 시작
 * 4. KeyTask에서 EXTI 활성화
 * 5. KeyTask가 Semaphore Take 대기
 * 6. B1 버튼 인터럽트 발생
 * 7. ISR에서 GiveFromISR()
 * 8. KeyTask가 Ready → Running
 * 9. LED Toggle + UART "[key]"
 * 10. 다음 버튼 입력 대기
 */

UART_HandleTypeDef huart3;

SemaphoreHandle_t keySemaphore = NULL;

/* 함수 선언 */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void KeyTask(void *argument);
void Error_Handler(void);

/* =========================================
 * 1. main
 * ========================================= */
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART3_UART_Init();

    /* Binary Semaphore 생성: 초기 0 */
    keySemaphore = xSemaphoreCreateBinary();

    if (keySemaphore == NULL)
    {
        Error_Handler();
    }

    /* KeyTask 생성 */
    if (xTaskCreate(
            KeyTask,
            "KeyTask",
            256,
            NULL,
            2,
            NULL
        ) != pdPASS)
    {
        Error_Handler();
    }

    /* FreeRTOS Scheduler 시작 */
    vTaskStartScheduler();

    /* 정상이라면 도달하지 않음 */
    Error_Handler();

    while (1)
    {
    }
}

/* =========================================
 * 2. KeyTask
 * ========================================= */
static void KeyTask(void *argument)
{
    const uint8_t msg[] = "[key]\r\n";

    (void)argument;

    /*
     * Scheduler 시작 이후 EXTI 활성화
     * Semaphore와 Task가 이미 생성된 상태
     */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);

    HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);

    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    for (;;)
    {
        /*
         * Semaphore = 0
         * → KeyTask Running → Blocked
         */
        if (xSemaphoreTake(
                keySemaphore,
                portMAX_DELAY
            ) == pdTRUE)
        {
            /*
             * 버튼 입력 안정화
             * vTaskDelay 실행 시 Blocked
             */
            vTaskDelay(pdMS_TO_TICKS(20));

            /*
             * 실제 버튼 눌림 확인
             * STM32F429ZI B1: Active High
             */
            if (HAL_GPIO_ReadPin(
                    GPIOC,
                    GPIO_PIN_13
                ) == GPIO_PIN_SET)
            {
                /* LED 상태 반전 */
                HAL_GPIO_TogglePin(
                    GPIOB,
                    GPIO_PIN_7
                );

                /* UART 메시지 출력 */
                HAL_UART_Transmit(
                    &huart3,
                    (uint8_t *)msg,
                    sizeof(msg) - 1,
                    100
                );

                /*
                 * 버튼을 누르고 있는 동안
                 * 중복 처리하지 않음
                 */
                while (HAL_GPIO_ReadPin(
                           GPIOC,
                           GPIO_PIN_13
                       ) == GPIO_PIN_SET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                /* 버튼 해제 안정화 */
                vTaskDelay(pdMS_TO_TICKS(20));

                /*
                 * 해제 후에도 눌린 상태라면
                 * 다시 해제 대기
                 */
                while (HAL_GPIO_ReadPin(
                           GPIOC,
                           GPIO_PIN_13
                       ) == GPIO_PIN_SET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                /*
                 * 대기 중 발생한 잔여 신호 제거
                 * Binary Semaphore이므로 최대 1개
                 */
                (void)xSemaphoreTake(
                    keySemaphore,
                    0
                );
            }
        }
    }
}

/* =========================================
 * 3. EXTI Callback (ISR Context)
 * ========================================= */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        BaseType_t xHigherPriorityTaskWoken =
            pdFALSE;

        /*
         * ISR에서 Binary Semaphore Give
         *
         * KeyTask:
         * Blocked → Ready
         */
        xSemaphoreGiveFromISR(
            keySemaphore,
            &xHigherPriorityTaskWoken
        );

        /*
         * 높은 우선순위 Task가 깨어났다면
         * PendSV를 통한 Context Switching 요청
         */
        portYIELD_FROM_ISR(
            xHigherPriorityTaskWoken
        );
    }
}

/* =========================================
 * 4. GPIO Init
 * ========================================= */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LD2 = PB7 */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_7,
        GPIO_PIN_RESET
    );

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* B1 = PC13 */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*
     * EXTI15_10 Interrupt Priority
     *
     * FreeRTOSConfig.h 설정에 따라
     * FromISR API 사용 가능한 우선순위여야 함
     */
    HAL_NVIC_SetPriority(
        EXTI15_10_IRQn,
        5,
        0
    );

    /*
     * 여기서는 IRQ 활성화하지 않음
     *
     * KeyTask 시작 시 활성화
     */
}

/* =========================================
 * 5. USART3 Init
 * ========================================= */
static void MX_USART3_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    /* PD8 = TX, PD9 = RX */
    GPIO_InitStruct.Pin =
        GPIO_PIN_8 | GPIO_PIN_9;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Alternate =
        GPIO_AF7_USART3;

    HAL_GPIO_Init(
        GPIOD,
        &GPIO_InitStruct
    );

    huart3.Instance = USART3;

    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;

    huart3.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart3.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =========================================
 * 6. Clock: HSI 16MHz
 * ========================================= */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(
            &RCC_OscInitStruct
        ) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0
        ) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =========================================
 * 7. Error Handler
 * ========================================= */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
