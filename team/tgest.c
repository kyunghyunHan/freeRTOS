#include "main.h"

#include "FreeRTOS.h"

#include "task.h"

#include "semphr.h"

#include "queue.h"

#include <stdint.h>

#include <string.h>

/*

 * NUCLEO-F429ZI / STM32F429ZI / FreeRTOS 기본 API 사용

 * B1 버튼: PC13, 상승 에지 인터럽트 (누르면 HIGH)

 * LD1: PB0 (@L11 켜기, @L10 끄기), LD2: PB7 (@LD2 상태 반전)

 * USART3: PD8(송신), PD9(수신), 115200 8N1, 수신 인터럽트 사용

 * 수동형 피에조 부저: PA6 (TIM3_CH1), 262Hz를 약 200ms 동안 출력

 * 내부 HSI 클록 16MHz 사용, 시스템 및 주변장치 버스 클록도 16MHz로 설정

 *

 * 두 태스크의 시작 동기화 (스케줄러 실행 후 한 번만 수행):

 *   KeyTask -> startUartSemaphore -> UartTask -> startKeySemaphore -> KeyTask

 *

 * 버튼 처리: EXTI 인터럽트 → keySemaphore 전달 → 버튼 떨림 제거 → PWM 부저 출력

 * UART 처리: USART3 인터럽트 → 명령 큐 및 세마포어 → 에코백 및 명령 실행

 *

 * 주의: STM32CubeIDE가 생성한 stm32f4xx_it.c에는 실제 인터럽트 핸들러

 * EXTI15_10_IRQHandler와 USART3_IRQHandler가 구현되어 있어야 함.

 * FreeRTOS의 인터럽트 우선순위 설정을 확인할 것. 우선순위 비트 4개인

 * 환경에서 우선순위 5로 설정한 ISR이 FromISR API를 호출할 수 있어야 함.

 */

#define UART_CMD_MAX          32U

#define UART_QUEUE_LENGTH      8U

#define BUZZER_ARR          3816U  /* 타이머 클록 16MHz ÷ 16 ÷ 3817 ≈ 261.985Hz */

#define BUZZER_CCR          1908U

/* CubeMX가 핸들을 다른 파일에서 정의한 경우 중복 정의하지 않도록 주의. */

UART_HandleTypeDef huart3;

TIM_HandleTypeDef htim3;

typedef struct

{

    char text[UART_CMD_MAX];

} CommandMessage;

static SemaphoreHandle_t keySemaphore;

static SemaphoreHandle_t commandSemaphore;

static SemaphoreHandle_t startUartSemaphore;

static SemaphoreHandle_t startKeySemaphore;

static QueueHandle_t commandQueue;

static uint8_t uartRxByte;

static char irqLine[UART_CMD_MAX];

static uint8_t irqLineLength;

static uint8_t irqDiscardUntilEnter;

void SystemClock_Config(void);

static void MX_GPIO_Init(void);

static void MX_USART3_UART_Init(void);

static void MX_TIM3_Init(void);

static void KeyTask(void *argument);

static void UartTask(void *argument);

static void UART_Write(const char *message);

void Error_Handler(void);

int main(void)

{

    /* ① 스케줄러를 시작하기 전에 MCU 주변장치를 초기화한다. */

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();

    MX_USART3_UART_Init();

    MX_TIM3_Init();

    /* ② 이벤트 세마포어는 빈 상태로 만들고, UART 명령은 큐에 저장하도록 준비한다. */

    keySemaphore       = xSemaphoreCreateBinary();

    commandSemaphore   = xSemaphoreCreateCounting(UART_QUEUE_LENGTH, 0);

    startUartSemaphore = xSemaphoreCreateBinary();

    startKeySemaphore  = xSemaphoreCreateBinary();

    commandQueue       = xQueueCreate(UART_QUEUE_LENGTH, sizeof(CommandMessage));

    if (keySemaphore == NULL || commandSemaphore == NULL ||

        startUartSemaphore == NULL || startKeySemaphore == NULL ||

        commandQueue == NULL)

    {

        Error_Handler();

    }

    /* ③ 우선순위가 같은 두 태스크를 생성한다. 각 태스크는 별도의 TCB와 스택을 가진다. */

    if (xTaskCreate(KeyTask, "KeyTask", 384, NULL, 2, NULL) != pdPASS)

    {

        Error_Handler();

    }

    if (xTaskCreate(UartTask, "UartTask", 512, NULL, 2, NULL) != pdPASS)

    {

        Error_Handler();

    }

    /* ④ 스케줄러 시작 시 SVC와 예외 복귀 과정을 통해 첫 태스크의 실행 문맥을 복원한다. */

    vTaskStartScheduler();

    Error_Handler();

    for (;;) { }

}

static void KeyTask(void *argument)

{

    (void)argument;

    /* ⑤ 시작 단계에서 단 한 번 수행하는 양방향 태스크 동기화: 버튼 태스크 → UART 태스크 → 버튼 태스크. */

    (void)xSemaphoreGive(startUartSemaphore);

    (void)xSemaphoreTake(startKeySemaphore, portMAX_DELAY);

    /* 세마포어와 스케줄러가 준비된 다음 인터럽트를 활성화해 시작 전 FromISR 호출을 방지한다. */

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);

    HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);

    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    for (;;)

    {

        /* ⑥ 버튼을 누르기 전까지 BLOCKED 상태로 대기하고, 인터럽트가 세마포어를 전달하면 깨어난다. */

        if (xSemaphoreTake(keySemaphore, portMAX_DELAY) != pdTRUE)

        {

            continue;

        }

        /* ⑦ 인터럽트 내부가 아니라 태스크에서 20ms 지연으로 버튼 접점 떨림을 제거한다. */

        vTaskDelay(pdMS_TO_TICKS(20));

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) != GPIO_PIN_SET)

        {

            continue;

        }

        /* ⑧ 수동형 피에조 부저에 약 262Hz PWM을 200ms 동안 출력한다. */

        __HAL_TIM_SET_AUTORELOAD(&htim3, BUZZER_ARR);

        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, BUZZER_CCR);

        __HAL_TIM_SET_COUNTER(&htim3, 0);

        if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)

        {

            Error_Handler();

        }

        vTaskDelay(pdMS_TO_TICKS(200));

        if (HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1) != HAL_OK)

        {

            Error_Handler();

        }

        /* 버튼을 놓을 때까지 기다려 한 번 누를 때 소리가 한 번만 나도록 한다. */

        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)

        {

            vTaskDelay(pdMS_TO_TICKS(10));

        }

        vTaskDelay(pdMS_TO_TICKS(20));

        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)

        {

            vTaskDelay(pdMS_TO_TICKS(10));

        }

        (void)xSemaphoreTake(keySemaphore, 0);

    }

}

static void UartTask(void *argument)

{

    CommandMessage command;

    (void)argument;

    /* ⑨ 시작 시 한 번만 수행하는 반대 방향 동기화: UART 태스크가 버튼 태스크에 신호를 돌려준다. */

    (void)xSemaphoreTake(startUartSemaphore, portMAX_DELAY);

    UART_Write("[SYNC] KeyTask -> UartTask\r\n");

    (void)xSemaphoreGive(startKeySemaphore);

    UART_Write("[SYNC] UartTask -> KeyTask\r\n");

    UART_Write("Commands: @L11 @L10 @LD2 @LD2ON @LD2OFF\r\n");

    /* 스케줄러 시작과 큐 생성이 완료된 후 UART 인터럽트 수신을 시작한다. */

    HAL_NVIC_ClearPendingIRQ(USART3_IRQn);

    HAL_NVIC_EnableIRQ(USART3_IRQn);

    if (HAL_UART_Receive_IT(&huart3, &uartRxByte, 1U) != HAL_OK)

    {

        Error_Handler();

    }

    for (;;)

    {

        /* ⑩ Enter를 받으면 UART 인터럽트가 완성된 명령을 큐에 넣고 세마포어를 전달한다. */

        if (xSemaphoreTake(commandSemaphore, portMAX_DELAY) != pdTRUE)

        {

            continue;

        }

        if (xQueueReceive(commandQueue, &command, 0) != pdPASS)

        {

            continue;  /* 세마포어와 큐의 상태가 일치하지 않으면 프로그램 처리에 문제가 있다는 뜻이다. */

        }

        /* ⑪ Enter 이후 입력 문자열을 에코백으로 전송한 다음 명령을 해석한다. */

        UART_Write("ECHO: ");

        UART_Write(command.text);

        UART_Write("\r\n");

        if (strcmp(command.text, "@L11") == 0)

        {

            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

            UART_Write("LD1 ON\r\n");

        }

        else if (strcmp(command.text, "@L10") == 0)

        {

            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

            UART_Write("LD1 OFF\r\n");

        }

        else if (strcmp(command.text, "@LD2") == 0)

        {

            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7);

            UART_Write("LD2 TOGGLE\r\n");

        }

        else if (strcmp(command.text, "@LD2ON") == 0)

        {

            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);

            UART_Write("LD2 ON\r\n");

        }

        else if (strcmp(command.text, "@LD2OFF") == 0)

        {

            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);

            UART_Write("LD2 OFF\r\n");

        }

        else

        {

            UART_Write("UNKNOWN COMMAND\r\n");

        }

    }

}

/*

 * ⑫ EXTI 콜백: 인터럽트에서 세마포어를 전달한다. 실제 EXTI15_10 인터럽트 핸들러는

 * stm32f4xx_it.c에서 HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13)을 호출해야 한다.

 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)

{

    if (GPIO_Pin == GPIO_PIN_13 && keySemaphore != NULL)

    {

        BaseType_t higherPriorityTaskWoken = pdFALSE;

        (void)xSemaphoreGiveFromISR(keySemaphore, &higherPriorityTaskWoken);

        portYIELD_FROM_ISR(higherPriorityTaskWoken);

    }

}

/*

 * ⑬ UART 인터럽트에서 문자를 한 바이트씩 받는다. ISR 내부에서는 블로킹 송신을 하지 않는다.

 * Enter 입력 시 완성된 문자열을 큐에 복사한 뒤 세마포어를 전달한다.

 * UART 수신 IRQ의 우선순위는 FreeRTOS FromISR API 사용 조건을 만족해야 한다.

 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)

{

    if (huart->Instance == USART3)

    {

        BaseType_t higherPriorityTaskWoken = pdFALSE;

        const uint8_t ch = uartRxByte;

        if (ch == '\r' || ch == '\n')

        {

            if (!irqDiscardUntilEnter && irqLineLength > 0U)

            {

                CommandMessage msg = { {0} };

                memcpy(msg.text, irqLine, irqLineLength);

                msg.text[irqLineLength] = '\0';

                if (xQueueSendFromISR(commandQueue, &msg,

                                      &higherPriorityTaskWoken) == pdPASS)

                {

                    (void)xSemaphoreGiveFromISR(commandSemaphore,

                                               &higherPriorityTaskWoken);

                }

            }

            irqLineLength = 0U;

            irqDiscardUntilEnter = 0U;

        }

        else if (!irqDiscardUntilEnter)

        {

            if (irqLineLength < (UART_CMD_MAX - 1U))

            {

                irqLine[irqLineLength++] = (char)ch;

            }

            else

            {

                /* 입력 길이가 초과된 명령은 Enter를 눌러도 실행하지 않는다. */

                irqDiscardUntilEnter = 1U;

                irqLineLength = 0U;

            }

        }

        /* 다음 한 바이트를 수신할 수 있도록 인터럽트 수신을 다시 등록한다. */

        (void)HAL_UART_Receive_IT(&huart3, &uartRxByte, 1U);

        portYIELD_FROM_ISR(higherPriorityTaskWoken);

    }

}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)

{

    if (huart->Instance == USART3)

    {

        /* 오버런 또는 잡음 오류를 처리한다. 오류로 중단되었을 수 있는 수신을 다시 시작한다. */

        __HAL_UART_CLEAR_OREFLAG(huart);

        (void)HAL_UART_Receive_IT(&huart3, &uartRxByte, 1U);

    }

}

static void UART_Write(const char *message)

{

    const size_t length = strlen(message);

    if (length != 0U)

    {

        /* UART 송신은 UartTask에서만 사용하므로 이 코드에서는 송신용 뮤텍스가 필요 없다. */

        (void)HAL_UART_Transmit(&huart3, (uint8_t *)message,

                                (uint16_t)length, 100U);

    }

}

static void MX_GPIO_Init(void)

{

    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LD1(PB0), LD2(PB7): UART 명령으로 제어할 LED를 출력 모드로 설정한다. */

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, GPIO_PIN_RESET);

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_7;

    gpio.Mode = GPIO_MODE_OUTPUT_PP;

    gpio.Pull = GPIO_NOPULL;

    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &gpio);

    /* B1(PC13): 누르면 HIGH가 되므로 상승 에지 EXTI 인터럽트로 설정한다. */

    gpio.Pin = GPIO_PIN_13;

    gpio.Mode = GPIO_MODE_IT_RISING;

    gpio.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOC, &gpio);

    /* xSemaphoreGiveFromISR()을 호출할 수 있는 NVIC 인터럽트 우선순위로 설정한다. */

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);

    /* 실제 IRQ 활성화는 스케줄러 시작 후 KeyTask에서 수행한다. */

}

static void MX_USART3_UART_Init(void)

{

    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    __HAL_RCC_USART3_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;

    gpio.Mode = GPIO_MODE_AF_PP;

    gpio.Pull = GPIO_PULLUP;

    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio.Alternate = GPIO_AF7_USART3;

    HAL_GPIO_Init(GPIOD, &gpio);

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

    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);

    /* UART IRQ 활성화는 스케줄러 시작 후 UartTask에서 수행한다. */

}

static void MX_TIM3_Init(void)

{

    GPIO_InitTypeDef gpio = {0};

    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    __HAL_RCC_TIM3_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6;

    gpio.Mode = GPIO_MODE_AF_PP;

    gpio.Pull = GPIO_NOPULL;

    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Alternate = GPIO_AF2_TIM3;

    HAL_GPIO_Init(GPIOA, &gpio);

    htim3.Instance = TIM3;

    htim3.Init.Prescaler = 15U;    /* TIM3 클록 16MHz를 16분주하여 카운터를 1MHz로 동작시킨다 */

    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;

    htim3.Init.Period = BUZZER_ARR;

    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)

    {

        Error_Handler();

    }

    oc.OCMode = TIM_OCMODE_PWM1;

    oc.Pulse = BUZZER_CCR;

    oc.OCPolarity = TIM_OCPOLARITY_HIGH;

    oc.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK)

    {

        Error_Handler();

    }

}

void SystemClock_Config(void)

{

    RCC_OscInitTypeDef osc = {0};

    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;

    osc.HSIState = RCC_HSI_ON;

    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;

    osc.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)

    {

        Error_Handler();

    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |

                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;

    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;

    clk.APB1CLKDivider = RCC_HCLK_DIV1;

    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)

    {

        Error_Handler();

    }

}

void Error_Handler(void)

{

    __disable_irq();

    for (;;) { }

}
