#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "cmsis_os.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define UART_CMD_MAX 32U
#define UART_QUEUE_LENGTH 8U
#define BUZZER_ARR 3816U /* 16MHz / (15+1) / (3816+1) = 약 262Hz */
#define BUZZER_CCR 1908U /* 약 50% 듀티비 */

UART_HandleTypeDef huart3;
TIM_HandleTypeDef htim3;

typedef struct
{
    char text[UART_CMD_MAX];
} CommandMessage;

/* 인터럽트와 태스크 사이 동기화 및 태스크 간 신호 전달 */
static SemaphoreHandle_t keySemaphore;
static SemaphoreHandle_t commandSemaphore;
static SemaphoreHandle_t startUartSemaphore;
static SemaphoreHandle_t startKeySemaphore;
static SemaphoreHandle_t beepRequestSemaphore;
static SemaphoreHandle_t keyReportSemaphore;
static SemaphoreHandle_t uartMutex;
/* Task2 ↔ Task3 반복 양방향 동기화용 세마포어 */
static SemaphoreHandle_t sem23;
static SemaphoreHandle_t sem32;
static QueueHandle_t commandQueue;

/* 9개 NumTask가 같은 함수를 사용하되 각각 자기 번호를 인수로 받는다. */
static uint32_t taskNumbers[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

/* UART 수신 인터럽트에서 하나씩 문자를 모으는 버퍼 */
static uint8_t uartRxByte;
static char irqLine[UART_CMD_MAX];
static uint8_t irqLineLength;
static uint8_t irqDiscardUntilEnter;

void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM3_Init(void);
static void KeyTask(void *argument);
static void UartTask(void *argument);
static void NumTask(void *argument);
static void UART_Write(const char *message);
static void Buzzer_Play(void);
void StartTask02(void const *argument);
void StartTask03(void const *argument);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    MX_TIM3_Init();

    /* 모든 세마포어는 초기 상태가 비어 있으며, 카운팅 세마포어도 0으로 시작한다. */
    keySemaphore = xSemaphoreCreateBinary();
    commandSemaphore = xSemaphoreCreateCounting(UART_QUEUE_LENGTH, 0);
    startUartSemaphore = xSemaphoreCreateBinary();
    startKeySemaphore = xSemaphoreCreateBinary();
    beepRequestSemaphore = xSemaphoreCreateBinary();
    keyReportSemaphore = xSemaphoreCreateBinary();
    uartMutex = xSemaphoreCreateMutex();
    sem23 = xSemaphoreCreateBinary();
    sem32 = xSemaphoreCreateBinary();
    commandQueue = xQueueCreate(UART_QUEUE_LENGTH, sizeof(CommandMessage));

    if (!keySemaphore || !commandSemaphore || !startUartSemaphore ||
        !startKeySemaphore || !beepRequestSemaphore || !keyReportSemaphore ||
        !uartMutex || !commandQueue || !sem23 || !sem32)
    {
        Error_Handler();
    }

    /* 핵심 태스크 우선순위를 NumTask보다 높게 지정해 명령 응답성을 확보한다. */
    if (xTaskCreate(KeyTask, "KeyTask", 384, NULL, 3, NULL) != pdPASS ||
        xTaskCreate(UartTask, "UartTask", 512, NULL, 3, NULL) != pdPASS)
    {
        Error_Handler();
    }

    /* 동일한 NumTask 함수를 인수만 달리하여 9개 생성한다. */
    for (uint32_t i = 0; i < 9U; i++)
    {
        if (xTaskCreate(NumTask, "NumTask", 256,
                        &taskNumbers[i], 1, NULL) != pdPASS)
        {
            Error_Handler();
        }
    }

    /* CMSIS-RTOS V1 방식으로 양방향 동기화 Task 2개 추가 */
    /* definition and creation of task2 */
    osThreadDef(task2, StartTask02, 2, 0, 256);
    osThreadId task2Handle = osThreadCreate(osThread(task2), NULL);

    /* definition and creation of task3 */
    osThreadDef(task3, StartTask03, 2, 0, 256);
    osThreadId task3Handle = osThreadCreate(osThread(task3), NULL);

    if (task2Handle == NULL || task3Handle == NULL)
    {
        Error_Handler();
    }

    srand(1234);
    vTaskStartScheduler();
    Error_Handler();
    for (;;)
    {
    }
}

/* 모든 태스크가 USART3를 함께 쓰므로 Mutex로 송신 구간을 보호한다. */
static void UART_Write(const char *message)
{
    size_t len = strlen(message);
    if (len == 0U)
        return;

    if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE)
    {
        (void)HAL_UART_Transmit(&huart3, (uint8_t *)message,
                                (uint16_t)len, 100U);
        (void)xSemaphoreGive(uartMutex);
    }
}

/* Task2 → Task3 → Task2 순서로 신호를 반복해서 주고받는다. */
void StartTask02(void const *argument)
{
    (void)argument;
    for (;;)
    {
        UART_Write("[Task2] Task3에 신호 전송\r\n");
        (void)xSemaphoreGive(sem23);

        if (xSemaphoreTake(sem32, portMAX_DELAY) == pdTRUE)
        {
            UART_Write("[Task2] Task3 응답 수신\r\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void StartTask03(void const *argument)
{
    (void)argument;
    for (;;)
    {
        if (xSemaphoreTake(sem23, portMAX_DELAY) == pdTRUE)
        {
            UART_Write("[Task3] Task2 신호 수신\r\n");
            vTaskDelay(pdMS_TO_TICKS(200));
            UART_Write("[Task3] Task2에 응답 전송\r\n");
            (void)xSemaphoreGive(sem32);
        }
    }
}

/* PWM 출력은 인터럽트에서 하지 않고 KeyTask 문맥에서 수행한다. */
static void Buzzer_Play(void)
{
    __HAL_TIM_SET_AUTORELOAD(&htim3, BUZZER_ARR);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, BUZZER_CCR);
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();

    vTaskDelay(pdMS_TO_TICKS(200));

    if (HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
}

static void KeyTask(void *argument)
{
    (void)argument;

    /* 시작 시 1회 양방향 동기화: KeyTask -> UartTask -> KeyTask */
    (void)xSemaphoreGive(startUartSemaphore);
    (void)xSemaphoreTake(startKeySemaphore, portMAX_DELAY);

    /* 세마포어와 스케줄러 준비 후 버튼 IRQ를 켠다. */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
    HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    for (;;)
    {
        /* UART에서 @BEEP가 들어오면 버튼 없이도 KeyTask가 부저를 울린다. */
        if (xSemaphoreTake(beepRequestSemaphore, 0) == pdTRUE)
        {
            Buzzer_Play();
        }

        /* 버튼 신호는 최대 10ms씩 대기하여 UART의 부저 요청도 처리한다. */
        if (xSemaphoreTake(keySemaphore, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) != GPIO_PIN_SET)
                continue;

            Buzzer_Play();

            /* KeyTask -> UartTask 방향의 반복 통신 */
            (void)xSemaphoreGive(keyReportSemaphore);

            /* 한 번 누를 때 한 번만 소리 나도록 버튼을 놓기를 기다린다. */
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
                vTaskDelay(pdMS_TO_TICKS(10));
            vTaskDelay(pdMS_TO_TICKS(20));
            (void)xSemaphoreTake(keySemaphore, 0);
        }
    }
}

static void UartTask(void *argument)
{
    CommandMessage command;
    char echoLine[UART_CMD_MAX + 12U];
    (void)argument;

    /* 시작 시 1회 양방향 동기화의 응답 */
    (void)xSemaphoreTake(startUartSemaphore, portMAX_DELAY);
    UART_Write("[SYNC] KeyTask -> UartTask\r\n");
    (void)xSemaphoreGive(startKeySemaphore);
    UART_Write("[SYNC] UartTask -> KeyTask\r\n");
    UART_Write("Commands: @L11 @L10 @LD2 @LD2ON @LD2OFF @BEEP\r\n");

    HAL_NVIC_ClearPendingIRQ(USART3_IRQn);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    if (HAL_UART_Receive_IT(&huart3, &uartRxByte, 1U) != HAL_OK)
        Error_Handler();

    for (;;)
    {
        /* KeyTask에서 버튼 누름을 알리면 UART로 결과를 송신한다. */
        if (xSemaphoreTake(keyReportSemaphore, 0) == pdTRUE)
        {
            UART_Write("[KEY] KEY PRESSED / BUZZER 262Hz\r\n");
        }

        /* UART Enter 입력 시 ISR이 명령 큐에 저장하고 세마포어를 준다. */
        if (xSemaphoreTake(commandSemaphore, pdMS_TO_TICKS(10)) != pdTRUE)
            continue;

        if (xQueueReceive(commandQueue, &command, 0) != pdPASS)
            continue;

        /* 에코백 문자열을 한 번에 보내서 NumTask 출력이 끼어들지 않게 한다. */
        (void)snprintf(echoLine, sizeof(echoLine), "ECHO: %s\r\n", command.text);
        UART_Write(echoLine);

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
        else if (strcmp(command.text, "@BEEP") == 0)
        {
            /* UartTask -> KeyTask 방향의 반복 통신 */
            (void)xSemaphoreGive(beepRequestSemaphore);
            UART_Write("BUZZER REQUEST SENT\r\n");
        }
        else
        {
            UART_Write("UNKNOWN COMMAND\r\n");
        }
    }
}

static void NumTask(void *argument)
{
    uint32_t number = *(const uint32_t *)argument;
    char msg[64];

    for (;;)
    {
        uint32_t delayTime;
        int len;

        /* rand()는 공유 상태를 사용하므로 Mutex 안에서 호출한다. */
        if (xSemaphoreTake(uartMutex, portMAX_DELAY) == pdTRUE)
        {
            delayTime = 100U + (uint32_t)(rand() % 401);
            len = snprintf(msg, sizeof(msg), "Task %lu : %lu\r\n",
                           (unsigned long)number, (unsigned long)number);
            if (len > 0 && len < (int)sizeof(msg))
            {
                (void)HAL_UART_Transmit(&huart3, (uint8_t *)msg,
                                        (uint16_t)len, 100U);
            }
            (void)xSemaphoreGive(uartMutex);
        }
        else
        {
            delayTime = 100U;
        }
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
}

/* 버튼 ISR에서는 태스크를 깨우는 작업만 수행한다. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13 && keySemaphore != NULL)
    {
        BaseType_t woken = pdFALSE;
        (void)xSemaphoreGiveFromISR(keySemaphore, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/* UART에서 문자를 1바이트씩 모으고, Enter 입력 시 큐에 전달한다. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3)
        return;

    BaseType_t woken = pdFALSE;
    uint8_t ch = uartRxByte;

    if (ch == '\r' || ch == '\n')
    {
        if (!irqDiscardUntilEnter && irqLineLength > 0U)
        {
            CommandMessage msg = {{0}};
            memcpy(msg.text, irqLine, irqLineLength);
            msg.text[irqLineLength] = '\0';

            if (xQueueSendFromISR(commandQueue, &msg, &woken) == pdPASS)
                (void)xSemaphoreGiveFromISR(commandSemaphore, &woken);
        }
        irqLineLength = 0U;
        irqDiscardUntilEnter = 0U;
    }
    else if (!irqDiscardUntilEnter)
    {
        if (irqLineLength < UART_CMD_MAX - 1U)
            irqLine[irqLineLength++] = (char)ch;
        else
        {
            /* 버퍼 길이를 넘은 명령은 Enter까지 버린다. */
            irqDiscardUntilEnter = 1U;
            irqLineLength = 0U;
        }
    }

    (void)HAL_UART_Receive_IT(&huart3, &uartRxByte, 1U);
    portYIELD_FROM_ISR(woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        (void)HAL_UART_Receive_IT(&huart3, &uartRxByte, 1U);
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* F429ZI 보드의 B1은 누르면 PC13이 HIGH가 된다. */
    gpio.Pin = GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_IT_RISING;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
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
        Error_Handler();
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
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
    htim3.Init.Prescaler = 15U;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = BUZZER_ARR;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
        Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = BUZZER_CCR;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
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
        Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;)
    {
    }
}
