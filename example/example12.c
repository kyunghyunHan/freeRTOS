/* NUCLEO-F429ZI / STM32F429xx / FreeRTOS Native API
 * 독립 실행: 이 파일 하나를 main.c로 사용한다. 다른 exampleXX.c와 동시 링크 금지.
 * 태스크 stack=512 words(2048 bytes), priority는 각 xTaskCreate 인수 참조.
 * 1. 길이3인 큐 두 개를 길이6인 queue set에 등록한다.
 * 2. consumer(2)가 set에서 기다리고 선택된 큐에서 정확히 한 항목을 꺼낸다.
 * 공통 실행 조건: preemption=1, time slicing=1, tick=1ms.
 * 같은 priority의 Ready 태스크는 tick마다 교대 가능, Blocked 태스크는 대상에서 제외.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
#include "stm32f4xx.h"
#include <stdint.h>
#include <stdio.h>

/* 이 파일 안에서만 쓰는 UART 출력: 별도 BSP/표준 입출력 retarget 불필요.
 * USART3=0x40004800, SR(+0x00)의 TXE(bit7), DR(+0x04).
 * TXE 검사와 DR 기록만 critical section으로 보호한다. 대기는 밖에서 한다.
 * 메시지 전체를 잠그지 않으므로 태스크 간 문자열은 섞일 수 있다.
 * Example20은 mutex, Example21은 gatekeeper로 메시지 단위 보호를 학습한다.
 * ISR에서는 호출 금지. Timer callback의 폴링 시간만큼 daemon 처리가 늦어진다.
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

/* assert 식은 검사만 수행한다. 실패 위치는 debugger에서 확인한다. */
volatile const char *failure_file;
volatile int failure_line;
void firmware_assert(const char *file, int line)
{
    __disable_irq();
    failure_file = file;
    failure_line = line;
    GPIOB->BSRR = (1U << 14); /* LD3 빨강; 오류 처리 중 UART/heap 사용 금지 */
    for (;;)
    {
        __NOP();
    }
}
void vApplicationMallocFailedHook(void) { firmware_assert(__FILE__, __LINE__); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    firmware_assert(__FILE__, __LINE__);
}

static void hardware_init(void)
{
    /* 1. RCC=0x40023800. 외부 크리스털/ST-LINK MCO에 의존하지 않는 HSI 16MHz.
     * SYSCLK=HCLK=PCLK1=PCLK2=16MHz. scheduler 시작 후에는 클록 변경 금지.
     */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
    }
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    SystemCoreClockUpdate();
    SCB->VTOR = FLASH_BASE; /* 기존 startup 벡터: 0x08000000, bootloader 없음 */
    __DSB();
    __ISB();
    NVIC_SetPriorityGrouping(0U); /* 4비트 모두 선점 우선순위 */

    /* 2. AHB1ENR(+0x30): GPIOB/C/D 클록, read-back으로 전파 대기.
     * GPIOB=0x40020400: MODER(+0), OTYPER(+4), PUPDR(+0x0c),
     * ODR(+0x14), BSRR(+0x18): 하위16 set / 상위16 reset.
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;
    GPIOB->BSRR = ((1U << 0) | (1U << 7) | (1U << 14)) << 16;
    GPIOB->MODER = (GPIOB->MODER & ~((3U << 0) | (3U << 14) | (3U << 28))) | (1U << 0) |
                   (1U << 14) | (1U << 28);
    GPIOB->OTYPER &= ~((1U << 0) | (1U << 7) | (1U << 14));
    GPIOB->PUPDR &= ~((3U << 0) | (3U << 14) | (3U << 28));
    GPIOC->MODER &= ~(3U << 26); /* PC13 입력: 버튼 눌림=1 */
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << 26)) | (2U << 26);

    /* 3. PD8 TX, PD9 RX: MODER=10(AF), AFRH AF7, push-pull, RX pull-up. */
    GPIOD->MODER = (GPIOD->MODER & ~((3U << 16) | (3U << 18))) | (2U << 16) | (2U << 18);
    GPIOD->AFR[1] = (GPIOD->AFR[1] & ~0xffU) | 0x77U;
    GPIOD->OTYPER &= ~((1U << 8) | (1U << 9));
    GPIOD->OSPEEDR = (GPIOD->OSPEEDR & ~((3U << 16) | (3U << 18))) | (2U << 16) | (2U << 18);
    GPIOD->PUPDR = (GPIOD->PUPDR & ~((3U << 16) | (3U << 18))) | (1U << 18);
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;
    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;
    /* OVER8=0: BRR=round(PCLK1/115200)=139=0x008b.
     * 실제 115107.9 baud(-0.08%, HSI 자체 오차 제외), 8 data/no parity/1 stop.
     * RX는 활성화하지만 이 예제들은 송신만 사용한다(RXNE IRQ 없음).
     */
    USART3->BRR = (16000000U + 115200U / 2U) / 115200U;
    USART3->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/* Chapter 4 — Example 12: Queue sets */
/* configUSE_QUEUE_SETS=1: example/FreeRTOSConfig.h에 설정되어 있다. */
static QueueHandle_t qa, qb;
static QueueSetHandle_t set;
static void producer(void *arg)
{
    (void)arg;
    uint32_t a = 0, b = 100;
    for (;;)
    {
        if (xQueueSend(qa, &a, 0) == pdPASS)
            a++;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (xQueueSend(qb, &b, 0) == pdPASS)
            b++;
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
static void consumer(void *arg)
{
    (void)arg;
    for (;;)
    {
        QueueSetMemberHandle_t selected = xQueueSelectFromSet(set, portMAX_DELAY);
        uint32_t v;
        if (selected != NULL && xQueueReceive((QueueHandle_t)selected, &v, 0) == pdPASS)
        {
            char buf[65];
            (void)snprintf(buf, sizeof buf, "Queue %c: %lu\r\n", selected == qa ? 'A' : 'B',
                           (unsigned long)v);
            uart_write(buf);
        }
    }
}
int main(void)
{
    hardware_init();
    qa = xQueueCreate(3, sizeof(uint32_t));
    qb = xQueueCreate(3, sizeof(uint32_t));
    configASSERT(qa && qb);
    set = xQueueCreateSet(6);
    configASSERT(set);
    {
        BaseType_t result0 = xQueueAddToSet(qa, set);
        if (result0 != pdPASS)
            firmware_assert(__FILE__, __LINE__);
        BaseType_t result1 = xQueueAddToSet(qb, set);
        if (result1 != pdPASS)
            firmware_assert(__FILE__, __LINE__);
    }
    {
        BaseType_t result0 = xTaskCreate(producer, "producer", 512, NULL, 1, NULL);
        if (result0 != pdPASS)
            firmware_assert(__FILE__, __LINE__);
    }
    {
        BaseType_t result0 = xTaskCreate(consumer, "consumer", 512, NULL, 2, NULL);
        if (result0 != pdPASS)
            firmware_assert(__FILE__, __LINE__);
    }

    /* 마지막 단계: 포트가 SVC로 첫 태스크를 시작하고 SysTick/PendSV를 관리한다. */
    vTaskStartScheduler();
    firmware_assert(__FILE__, __LINE__); /* heap 부족 등 scheduler 반환은 오류 */
    return 0;
}
