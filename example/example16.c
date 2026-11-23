
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include <stdint.h>

/*
STM32F429ZI - Chapter 6 Example 16

소프트웨어 인터럽트 + Binary Semaphore

USART3 : PD8 TX / PD9 RX
Baud   : 115200
LED    : PB0

Periodic Task : Priority 1
Handler Task  : Priority 3

① Binary Semaphore 생성 (초기 0)
② Handler Task / Periodic Task 생성
③ Scheduler 시작
④ Handler Task → Semaphore Take → BLOCKED
⑤ Periodic Task → 500ms 대기
⑥ Periodic Task → 소프트웨어 인터럽트 발생
⑦ ISR → Semaphore Give
⑧ Handler Task → READY
⑨ PendSV → Handler Task RUNNING
⑩ Handler Task → 이벤트 처리
⑪ Periodic Task 실행 재개
⑫ 반복

소프트웨어 인터럽트용으로
사용하지 않는 HASH_RNG IRQ를 예약하여 사용한다.
실제 HASH/RNG 주변장치는 사용하지 않는다.
*/

static SemaphoreHandle_t binarySemaphore = NULL;


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
오류 발생 시 정지
*/
static void firmware_error(void)
{
    __disable_irq();

    GPIOB->BSRR = (1U << 14);

    while (1)
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

Semaphore를 기다리다가
ISR에서 Give하면 이벤트 처리
*/
static void HandlerTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        /*
        초기 Semaphore = 0

        Handler Task
        RUNNING → BLOCKED
        */

        if (xSemaphoreTake(
                binarySemaphore,
                portMAX_DELAY) == pdTRUE)
        {
            /*
            ISR에서 Semaphore Give

            Handler Task
            BLOCKED → READY → RUNNING

            이벤트 처리
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

        Periodic Task
        RUNNING → BLOCKED

        500ms 경과 후
        BLOCKED → READY
        */

        vTaskDelay(delay500ms);


        /*
        인터럽트 발생 직전 메시지
        */

        uart_write(
            "Periodic task - About to generate an interrupt.\r\n"
        );


        /*
        소프트웨어 인터럽트 발생

        NVIC Pending Bit 설정

        HASH_RNG_IRQHandler() 실행

        실제 HASH/RNG 주변장치의
        하드웨어 이벤트는 사용하지 않는다.
        */

        NVIC_SetPendingIRQ(HASH_RNG_IRQn);


        /*
        인터럽트 이후 메시지

        Handler Task의 우선순위가 더 높으므로
        정상적인 선점 동작에서는
        Handler Task의 출력이 먼저 나타난다.
        */

        uart_write(
            "Periodic task - Interrupt generated.\r\n\r\n"
        );
    }
}


/*
소프트웨어 인터럽트 ISR

교재의 ulExampleInterruptHandler() 역할

HASH_RNG IRQ를 소프트웨어 인터럽트용으로 사용
*/
void HASH_RNG_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken =
        pdFALSE;

    /*
    ISR에서 Binary Semaphore Give

    Handler Task
    BLOCKED → READY

    Handler Task Priority = 3
    Periodic Task Priority = 1
    */

    (void)xSemaphoreGiveFromISR(
        binarySemaphore,
        &xHigherPriorityTaskWoken
    );


    /*
    높은 우선순위 Task가 깨어났다면
    PendSV 요청

    ISR 종료 후 Context Switching
    */

    portYIELD_FROM_ISR(
        xHigherPriorityTaskWoken
    );
}


/*
main

Semaphore 생성
Task 생성
소프트웨어 IRQ 설정
Scheduler 시작
*/
int main(void)
{
    hardware_init();


    /*
    ① Binary Semaphore 생성

    초기 Count = 0
    */

    binarySemaphore = xSemaphoreCreateBinary();

    if (binarySemaphore == NULL)
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

    IRQ 우선순위 = 6

    FreeRTOS FromISR API 호출이
    허용되는 우선순위여야 한다.
    */

    NVIC_DisableIRQ(HASH_RNG_IRQn);

    NVIC_ClearPendingIRQ(HASH_RNG_IRQn);

    NVIC_SetPriority(
        HASH_RNG_IRQn,
        6U
    );

    NVIC_EnableIRQ(HASH_RNG_IRQn);


    /*
    ⑤ Scheduler 시작

    Handler Task가 우선 실행된다.

    Semaphore = 0이므로 BLOCKED

    이후 Periodic Task 실행

    500ms마다 소프트웨어 인터럽트 발생
    */

    vTaskStartScheduler();

    firmware_error();

    return 0;
}
