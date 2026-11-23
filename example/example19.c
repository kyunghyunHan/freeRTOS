
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "stm32f4xx.h"
#include <stdint.h>

/*
STM32F429ZI - FreeRTOS Chapter 6 Example 19

Sending and Receiving on a Queue
From Within an Interrupt

USART3 : PD8 TX / PD9 RX
Baud   : 115200
LED    : PB0

Integer Generator Task : Priority 1
String Printer Task    : Priority 2

Integer Queue : 10 x uint32_t
String Queue  : 10 x char *

200ms마다 Integer Generator가 정수 5개 전송

ISR에서 정수 5개 수신
하위 2비트로 문자열 선택
String Queue에 문자열 포인터 전송

Printer Task에서 문자열 출력

소프트웨어 IRQ : HASH_RNG_IRQn
*/

static QueueHandle_t xIntegerQueue = NULL;
static QueueHandle_t xStringQueue = NULL;

static volatile uint32_t queueErrorCount = 0;


/*
기존 FreeRTOSConfig.h에서
Trace Hook이 활성화된 경우 사용
*/
void Trace_OnSwitchIn(void *tcb)
{
    (void)tcb;
}


/*
configUSE_TIMERS가 활성화되고
Static Allocation을 사용하는 경우

Timer Daemon Task 메모리 제공
*/
#if (configUSE_TIMERS == 1) && \
    (configSUPPORT_STATIC_ALLOCATION == 1)

void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t timerTaskTCB;

    static StackType_t timerTaskStack[
        configTIMER_TASK_STACK_DEPTH
    ];

    *ppxTimerTaskTCBBuffer = &timerTaskTCB;

    *ppxTimerTaskStackBuffer = timerTaskStack;

    *pulTimerTaskStackSize =
        configTIMER_TASK_STACK_DEPTH;
}

#endif


/*
UART 문자열 출력

TXE가 1이면 DR에 데이터 기록
ISR에서는 사용하지 않는다.
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
    PB0 LED
    PB14 Error LED
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
    USART3 GPIO

    PD8 TX
    PD9 RX

    AF7
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
    USART3 115200 8N1
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
교재 Listing 107

Integer Generator Task

Priority = 1

200ms마다 정수 5개를 생성해
Integer Queue에 전송

이후 Software IRQ 발생
*/
static void vIntegerGenerator(void *argument)
{
    TickType_t xLastExecutionTime;

    uint32_t ulValueToSend = 0U;

    (void)argument;


    /*
    주기 실행 기준 Tick 설정
    */

    xLastExecutionTime = xTaskGetTickCount();


    for (;;)
    {
        /*
        200ms 주기 대기

        RUNNING → BLOCKED

        Tick 경과 후
        BLOCKED → READY
        */

        vTaskDelayUntil(
            &xLastExecutionTime,
            pdMS_TO_TICKS(200)
        );


        /*
        Integer Queue에
        정수 5개 전송

        첫 번째 반복

        0, 1, 2, 3, 4

        두 번째 반복

        5, 6, 7, 8, 9
        */

        for (uint32_t i = 0U; i < 5U; i++)
        {
            if (xQueueSendToBack(
                    xIntegerQueue,
                    &ulValueToSend,
                    0) != pdPASS)
            {
                queueErrorCount++;
                firmware_error();
            }

            ulValueToSend++;
        }


        /*
        소프트웨어 인터럽트 발생 직전
        */

        uart_write(
            "Generator task - About to generate an interrupt.\r\n"
        );


        /*
        NVIC Pending 설정

        HASH_RNG_IRQHandler 실행
        */

        NVIC_SetPendingIRQ(HASH_RNG_IRQn);


        /*
        String Printer Priority = 2

        Integer Generator Priority = 1

        ISR 종료 후 Printer Task가
        먼저 실행되는 것이 정상적인 흐름
        */

        uart_write(
            "Generator task - Interrupt generated.\r\n\r\n"
        );
    }
}


/*
교재 Listing 108

Software Interrupt Handler

ISR에서 두 종류의 Queue API 사용

① xQueueReceiveFromISR()
② xQueueSendToBackFromISR()
*/
void HASH_RNG_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken =
        pdFALSE;

    uint32_t ulReceivedNumber;


    /*
    교재의 문자열 배열

    정수의 하위 2비트를 사용하여
    문자열 인덱스를 결정한다.
    */

    static const char *pcStrings[] =
    {
        "String 0\r\n",
        "String 1\r\n",
        "String 2\r\n",
        "String 3\r\n"
    };


    /*
    Integer Queue가 비워질 때까지
    정수를 반복 수신

    ISR은 Blocked 상태가 될 수 없다.
    */

    while (xQueueReceiveFromISR(
               xIntegerQueue,
               &ulReceivedNumber,
               &xHigherPriorityTaskWoken) == pdPASS)
    {
        /*
        하위 2비트만 추출

        0x03 = 0000 0011

        정수 0 → 0
        정수 1 → 1
        정수 2 → 2
        정수 3 → 3
        정수 4 → 0
        */

        ulReceivedNumber &= 0x03U;


        /*
        해당 문자열 포인터를
        String Queue에 전송

        문자열 본문이 아니라
        포인터 값을 Queue에 복사한다.
        */

        const char *pcString =
            pcStrings[ulReceivedNumber];

        if (xQueueSendToBackFromISR(
                xStringQueue,
                &pcString,
                &xHigherPriorityTaskWoken) != pdPASS)
        {
            queueErrorCount++;
        }
    }


    /*
    ISR에서 Queue 송신으로
    Printer Task가 깨어날 수 있다.

    Printer Task
    BLOCKED → READY

    더 높은 Priority라면
    PendSV Context Switching 요청
    */

    portYIELD_FROM_ISR(
        xHigherPriorityTaskWoken
    );
}


/*
교재 Listing 109

String Printer Task

Priority = 2

String Queue에서 문자열 포인터 수신
*/
static void vStringPrinter(void *argument)
{
    const char *pcString = NULL;

    (void)argument;


    for (;;)
    {
        /*
        String Queue가 비어 있으면

        Printer Task
        RUNNING → BLOCKED

        ISR에서 문자열 포인터가 도착하면

        BLOCKED → READY
        */

        if (xQueueReceive(
                xStringQueue,
                &pcString,
                portMAX_DELAY) == pdPASS)
        {
            /*
            Queue에서 수신한
            문자열을 UART로 출력

            이 코드는 ISR이 아니라
            Printer Task에서 실행된다.
            */

            uart_write(pcString);

            GPIOB->ODR ^= (1U << 0);
        }
    }
}


/*
교재 Listing 110

main

Queue 2개 생성
Task 2개 생성
소프트웨어 인터럽트 설정
Scheduler 시작
*/
int main(void)
{
    hardware_init();


    /*
    ① Integer Queue 생성

    최대 10개

    각 항목 = uint32_t
    */

    xIntegerQueue =
        xQueueCreate(
            10,
            sizeof(uint32_t)
        );

    if (xIntegerQueue == NULL)
    {
        firmware_error();
    }


    /*
    ② String Queue 생성

    최대 10개

    각 항목 = char *
    */

    xStringQueue =
        xQueueCreate(
            10,
            sizeof(const char *)
        );

    if (xStringQueue == NULL)
    {
        firmware_error();
    }


    /*
    ③ Integer Generator Task 생성

    Priority = 1
    Stack = 512 words
    */

    if (xTaskCreate(
            vIntegerGenerator,
            "IntGen",
            512,
            NULL,
            1,
            NULL) != pdPASS)
    {
        firmware_error();
    }


    /*
    ④ String Printer Task 생성

    Priority = 2
    Stack = 512 words
    */

    if (xTaskCreate(
            vStringPrinter,
            "String",
            512,
            NULL,
            2,
            NULL) != pdPASS)
    {
        firmware_error();
    }


    /*
    ⑤ 소프트웨어 인터럽트 설정

    HASH_RNG_IRQn

    NVIC Priority = 6
    */

    NVIC_DisableIRQ(HASH_RNG_IRQn);

    NVIC_ClearPendingIRQ(HASH_RNG_IRQn);

    NVIC_SetPriority(
        HASH_RNG_IRQn,
        6U
    );

    NVIC_EnableIRQ(HASH_RNG_IRQn);


    /*
    ⑥ Scheduler 시작

    Printer Task 먼저 실행

    String Queue Empty

    Printer Task → BLOCKED

    Generator Task 실행 후
    200ms 대기

    이후 인터럽트 처리 반복
    */

    vTaskStartScheduler();


    firmware_error();

    return 0;
}
