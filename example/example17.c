
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include <stdint.h>

/*
STM32F429ZI - FreeRTOS Chapter 6 Example 17

Counting Semaphore를 이용한 ISR과 Task 동기화

USART3 : PD8 TX / PD9 RX
Baud   : 115200
LED    : PB0

Periodic Task : Priority 1
Handler Task  : Priority 3

동작 순서

① Counting Semaphore 생성 (Max=10, Initial=0)
② Handler Task / Periodic Task 생성
③ Scheduler 시작
④ Handler Task → Take(0) → BLOCKED
⑤ Periodic Task → 500ms 대기
⑥ 소프트웨어 인터럽트 발생
⑦ ISR → GiveFromISR() 3회
⑧ Handler Task BLOCKED → READY
⑨ PendSV → Handler Task RUNNING
⑩ Handler Task → Take 3회 → 이벤트 3개 처리
⑪ Handler Task 다시 BLOCKED
⑫ Periodic Task 실행 재개
⑬ 500ms마다 반복

소프트웨어 인터럽트는 HASH_RNG_IRQn을 이용한다.

HASH/RNG 주변장치 자체는 사용하지 않는다.
이 IRQ가 다른 코드에서 사용되지 않아야 한다.
*/

static SemaphoreHandle_t countingSemaphore = NULL;
void Trace_OnSwitchIn(void *tcb)
{
    (void)tcb;
}

/*
UART 문자열 출력

USART3 TXE 확인 후 한 글자씩 전송한다.
ISR에서는 호출하지 않는다.
*/
static void uart_write(const char *text)
{
    while (*text != '\0')
    {
        taskENTER_CRITICAL();

        if ((USART3->SR & USART_SR_TXE) != 0U)
        {
            USART3->DR = (uint8_t)*text++;
        }

        taskEXIT_CRITICAL();
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

    while ((RCC->CFGR & RCC_CFGR_SWS)
           != RCC_CFGR_SWS_HSI)
    {
    }

    RCC->CFGR &= ~(
        RCC_CFGR_HPRE |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );

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
    USART3 GPIO 설정

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
Handler Task

Priority = 3

Counting Semaphore가 0이면 BLOCKED

ISR에서 Give하면 깨어나고
누적된 이벤트를 하나씩 처리한다.
*/
static void HandlerTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        /*
        Count = 0

        Handler Task
        RUNNING → BLOCKED

        Count가 증가하면
        BLOCKED → READY
        */

        if (xSemaphoreTake(
                countingSemaphore,
                portMAX_DELAY) == pdTRUE)
        {
            /*
            Take 성공

            이벤트 1개 소비

            남은 Count가 있으면
            다음 Take는 즉시 성공한다.
            */

            GPIOB->ODR ^= (1U << 0);

            uart_write(
                "Handler task - Processing event.\r\n"
            );
        }
    }
}


/*
Periodic Task

Priority = 1

500ms마다 소프트웨어 인터럽트를 발생시킨다.
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
        */

        vTaskDelay(delay500ms);


        /*
        소프트웨어 인터럽트 발생 직전
        */

        uart_write(
            "Periodic task - About to generate an interrupt.\r\n"
        );


        /*
        NVIC Pending Bit 설정

        HASH_RNG_IRQHandler() 실행
        */

        NVIC_SetPendingIRQ(HASH_RNG_IRQn);


        /*
        Handler Task가 높은 우선순위이므로
        일반적인 실행에서는
        이벤트 3개 처리 후 이 메시지가 출력된다.
        */

        uart_write(
            "Periodic task - Interrupt generated.\r\n\r\n"
        );
    }
}


/*
소프트웨어 인터럽트 ISR

교재 Example 17의 핵심

인터럽트 1회 발생 시
Semaphore Give를 3회 실행한다.
*/
void HASH_RNG_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken =
        pdFALSE;

    /*
    Counting Semaphore Give ①

    대기 중인 Handler Task가 있으면
    BLOCKED → READY
    */

    (void)xSemaphoreGiveFromISR(
        countingSemaphore,
        &xHigherPriorityTaskWoken
    );


    /*
    Counting Semaphore Give ②

    추가 이벤트 1개 누적
    */

    (void)xSemaphoreGiveFromISR(
        countingSemaphore,
        &xHigherPriorityTaskWoken
    );


    /*
    Counting Semaphore Give ③

    추가 이벤트 1개 누적
    */

    (void)xSemaphoreGiveFromISR(
        countingSemaphore,
        &xHigherPriorityTaskWoken
    );


    /*
    더 높은 우선순위 Task가 깨어났다면
    PendSV를 통한 Context Switching 요청
    */

    portYIELD_FROM_ISR(
        xHigherPriorityTaskWoken
    );
}


/*
main

Semaphore 생성
Task 생성
IRQ 설정
Scheduler 시작
*/
int main(void)
{
    hardware_init();


    /*
    ① Counting Semaphore 생성

    Max Count     = 10
    Initial Count = 0

    최대 10개의 이벤트를 누적할 수 있다.
    */

    countingSemaphore =
        xSemaphoreCreateCounting(10, 0);

    if (countingSemaphore == NULL)
    {
        firmware_error();
    }


    /*
    ② Handler Task 생성

    Priority = 3
    Stack = 512 words
    */

    if (xTaskCreate(
            HandlerTask,
            "Handler",
            512,
            NULL,
            3,
            NULL) != pdPASS)
    {
        firmware_error();
    }


    /*
    ③ Periodic Task 생성

    Priority = 1
    Stack = 512 words
    */

    if (xTaskCreate(
            PeriodicTask,
            "Periodic",
            512,
            NULL,
            1,
            NULL) != pdPASS)
    {
        firmware_error();
    }


    /*
    ④ 소프트웨어 인터럽트 설정

    IRQ Priority = 6
    */

    NVIC_DisableIRQ(HASH_RNG_IRQn);

    NVIC_ClearPendingIRQ(HASH_RNG_IRQn);

    NVIC_SetPriority(
        HASH_RNG_IRQn,
        6U
    );

    NVIC_EnableIRQ(HASH_RNG_IRQn);


    /*
    ⑤ FreeRTOS Scheduler 시작

    Handler Task 먼저 실행

    Take(0) → BLOCKED

    이후 Periodic Task가
    500ms마다 인터럽트를 발생시킨다.
    */

    vTaskStartScheduler();

    firmware_error();

    return 0;
}
