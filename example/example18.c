
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stm32f4xx.h"
#include <stdint.h>

/*
STM32F429ZI - FreeRTOS Chapter 6 Example 18

Centralized Deferred Interrupt Processing

USART3 : PD8 TX / PD9 RX
Baud   : 115200
LED    : PB0

Periodic Task : Priority 3
Daemon Task   : Priority 4

동작 순서

① 하드웨어 초기화
② Periodic Task 생성
③ 소프트웨어 IRQ 설정
④ Scheduler 시작
⑤ Daemon Task → Timer Queue 대기
⑥ Periodic Task → 500ms 대기
⑦ Periodic Task → 소프트웨어 IRQ 발생
⑧ ISR → xTimerPendFunctionCallFromISR()
⑨ Timer Command Queue에 실행 요청 저장
⑩ Daemon Task BLOCKED → READY
⑪ PendSV → Daemon Task RUNNING
⑫ Deferred 함수 실행 및 이벤트 번호 출력
⑬ Daemon Task 다시 대기
⑭ Periodic Task 실행 재개
⑮ 500ms마다 반복

별도 Handler Task 없음
Binary / Counting Semaphore 없음

소프트웨어 인터럽트는 HASH_RNG_IRQn 사용
HASH/RNG 주변장치 자체는 사용하지 않음
*/

static volatile uint32_t deferredErrorCount = 0;

/*
FreeRTOSConfig.h에 Trace Hook이
활성화된 경우를 위한 함수

Trace 분석은 수행하지 않는다.
*/
void Trace_OnSwitchIn(void *tcb)
{
    (void)tcb;
}
void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t timerTaskTCB;

    static StackType_t timerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &timerTaskTCB;

    *ppxTimerTaskStackBuffer = timerTaskStack;

    *pulTimerTaskStackSize =
        configTIMER_TASK_STACK_DEPTH;
}

/*
UART 문자열 출력

USART3 TXE 확인
한 글자씩 전송

ISR에서는 호출하지 않는다.
*/
static void uart_write(const char *text)
{
    while (*text != '\0')
    {
        while ((USART3->SR & USART_SR_TXE) == 0U)
        {
        }

        taskENTER_CRITICAL();

        if ((USART3->SR & USART_SR_TXE) != 0U)
        {
            USART3->DR = (uint8_t)*text++;
        }

        taskEXIT_CRITICAL();
    }
}

/*
32비트 정수를 문자열로 변환하여 출력
*/
static void uart_write_number(uint32_t value)
{
    char buffer[11];
    uint32_t index = 0;

    if (value == 0U)
    {
        uart_write("0");
        return;
    }

    while (value > 0U)
    {
        buffer[index++] =
            (char)('0' + (value % 10U));

        value /= 10U;
    }

    while (index > 0U)
    {
        char character[2];

        character[0] = buffer[--index];
        character[1] = '\0';

        uart_write(character);
    }
}

/*
오류 발생 시 PB14 LED ON
*/
static void firmware_error(void)
{
    __disable_irq();

    GPIOB->BSRR = (1U << 14);

    for (;;)
    {
        __NOP();
    }
}

/*
HSI 16MHz
GPIOB LED
USART3 초기화
*/
static void hardware_init(void)
{
    /*
    HSI 16MHz 설정
    */

    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    RCC->CFGR &= ~RCC_CFGR_SW;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
    }

    RCC->CFGR &= ~(
        RCC_CFGR_HPRE |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2);

    SystemCoreClockUpdate();

    SCB->VTOR = FLASH_BASE;

    __DSB();
    __ISB();

    NVIC_SetPriorityGrouping(0U);

    /*
    GPIOB / GPIOD Clock Enable
    */

    RCC->AHB1ENR |=
        RCC_AHB1ENR_GPIOBEN |
        RCC_AHB1ENR_GPIODEN;

    (void)RCC->AHB1ENR;

    /*
    PB0 LED 출력
    PB14 오류 LED 출력
    */

    GPIOB->BSRR =
        ((1U << 0) | (1U << 14)) << 16;

    GPIOB->MODER =
        (GPIOB->MODER &
         ~((3U << 0) | (3U << 28))) |
        (1U << 0) |
        (1U << 28);

    GPIOB->OTYPER &=
        ~((1U << 0) | (1U << 14));

    GPIOB->PUPDR &=
        ~((3U << 0) | (3U << 28));

    /*
    PD8 = TX
    PD9 = RX
    Alternate Function 7
    */

    GPIOD->MODER =
        (GPIOD->MODER &
         ~((3U << 16) | (3U << 18))) |
        (2U << 16) |
        (2U << 18);

    GPIOD->AFR[1] =
        (GPIOD->AFR[1] & ~0xFFU) |
        0x77U;

    GPIOD->OTYPER &=
        ~((1U << 8) | (1U << 9));

    GPIOD->PUPDR =
        (GPIOD->PUPDR &
         ~((3U << 16) | (3U << 18))) |
        (1U << 18);

    /*
    USART3 115200, 8N1
    */

    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    (void)RCC->APB1ENR;

    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;

    USART3->BRR =
        (16000000U + 115200U / 2U) /
        115200U;

    USART3->CR1 =
        USART_CR1_UE |
        USART_CR1_TE |
        USART_CR1_RE;
}

/*
교재 Listing 103

Deferred Handling Function

ISR에서 직접 실행하는 함수가 아니다.

Timer Command Queue를 통해
Daemon Task가 이 함수를 실행한다.

pvParameter1 : 사용하지 않음
ulParameter2 : 인터럽트 발생 번호
*/
static void vDeferredHandlingFunction(
    void *pvParameter1,
    uint32_t ulParameter2)
{
    (void)pvParameter1;

    /*
    Daemon Task 문맥에서 실행

    Thread Mode
    PSP 사용

    별도 Handler Task 없이
    인터럽트 후속 처리 수행
    */

    GPIOB->ODR ^= (1U << 0);

    uart_write(
        "Handler function - Processing event ");

    uart_write_number(ulParameter2);

    uart_write("\r\n");
}

/*
교재 Listing 102

Software Interrupt Handler

ISR은 이벤트를 직접 처리하지 않고
Timer Command Queue로 전달한다.
*/
void HASH_RNG_IRQHandler(void)
{
    static uint32_t ulParameterValue = 0U;

    BaseType_t xHigherPriorityTaskWoken =
        pdFALSE;

    /*
    xTimerPendFunctionCallFromISR()

    실행할 함수
    vDeferredHandlingFunction

    Parameter1 = NULL

    Parameter2 = ulParameterValue

    Timer Command Queue에
    실행 요청을 저장한다.
    */

    BaseType_t result =
        xTimerPendFunctionCallFromISR(
            vDeferredHandlingFunction,
            NULL,
            ulParameterValue,
            &xHigherPriorityTaskWoken);

    /*
    인터럽트 발생 번호 증가

    교재와 동일하게
    ISR 호출마다 1씩 증가한다.
    */

    ulParameterValue++;

    /*
    Timer Queue 전송에 실패하면
    실패 횟수를 기록한다.
    */

    if (result != pdPASS)
    {
        deferredErrorCount++;
    }

    /*
    Daemon Task가 높은 우선순위로
    깨어났으면 PendSV 요청
    */

    portYIELD_FROM_ISR(
        xHigherPriorityTaskWoken);
}

/*
Periodic Task

교재 Example 16과 동일한 구조

500ms마다 소프트웨어 인터럽트 발생
*/
static void PeriodicTask(void *argument)
{
    const TickType_t delay500ms =
        pdMS_TO_TICKS(500);

    (void)argument;

    for (;;)
    {
        /*
        500ms 대기

        RUNNING → BLOCKED

        시간이 지나면
        BLOCKED → READY
        */

        vTaskDelay(delay500ms);

        /*
        소프트웨어 인터럽트 발생 직전
        */

        uart_write(
            "Periodic task - About to generate an interrupt.\r\n");

        /*
        NVIC Pending Bit 설정

        HASH_RNG_IRQHandler 실행
        */

        NVIC_SetPendingIRQ(HASH_RNG_IRQn);

        /*
        Daemon Task Priority = 4

        Periodic Task Priority = 3

        Daemon Task가 먼저 실행되면
        Deferred 메시지가 먼저 출력된다.
        */

        uart_write(
            "Periodic task - Interrupt generated.\r\n\r\n");
    }
}

/*
main

하드웨어 초기화
Task 생성
IRQ 설정
Scheduler 시작
*/
int main(void)
{
    hardware_init();

    /*
    교재 Example 18에서는
    Semaphore를 생성하지 않는다.

    별도의 Handler Task도 생성하지 않는다.

    Daemon Task는 FreeRTOS가
    Scheduler 시작 과정에서 생성한다.
    */

    /*
    ① Periodic Task 생성

    Priority = 3
    Stack = 512 words
    */

    if (xTaskCreate(
            PeriodicTask,
            "Periodic",
            512,
            NULL,
            3,
            NULL) != pdPASS)
    {
        firmware_error();
    }

    /*
    ② 소프트웨어 인터럽트 설정

    IRQ Priority = 6

    FreeRTOS FromISR API 호출 가능
    우선순위 범위에 속해야 한다.
    */

    NVIC_DisableIRQ(HASH_RNG_IRQn);

    NVIC_ClearPendingIRQ(HASH_RNG_IRQn);

    NVIC_SetPriority(
        HASH_RNG_IRQn,
        6U);

    NVIC_EnableIRQ(HASH_RNG_IRQn);

    /*
    ③ FreeRTOS Scheduler 시작

    FreeRTOS 내부에서

    Idle Task 생성
    Timer Daemon Task 생성
    Timer Command Queue 사용

    Daemon Task Priority = 4

    Timer Queue에 실행 요청이 없으면
    Daemon Task는 BLOCKED
    */

    vTaskStartScheduler();

    /*
    Scheduler 시작 실패 시 실행
    */

    firmware_error();

    return 0;
}
