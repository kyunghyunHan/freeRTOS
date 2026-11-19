#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*
 * STM32F429ZI / Bare register / FreeRTOS
 * LD1 PB0: H has requested UART mutex (ON until acquired)
 * LD2 PB7: M currently owns UART mutex
 * USART3 TX PD8 AF7 / 115200 8N1 / HSI 16MHz
 *
 * H priority 3: delay 2s -> naturally preempt M -> immediately
 *               request mutex -> blocked until M releases.
 * M priority 2: take mutex -> busy work until 12s elapsed -> release.
 * Logger priority 1: print captured events after 14s.
 *
 * In FreeRTOSConfig.h (must be rebuilt with tasks.c):
 *   void Trace_OnSwitchIn(void *tcb);
 *   #define traceTASK_SWITCHED_IN() Trace_OnSwitchIn((void *)pxCurrentTCB)
 *   #define configUSE_PREEMPTION 1
 *   #define configUSE_MUTEXES 1
 *   #define configMAX_PRIORITIES 4 // or greater
 */

#define H_PRIORITY             (tskIDLE_PRIORITY + 3U)
#define M_PRIORITY             (tskIDLE_PRIORITY + 2U)
#define LOGGER_PRIORITY        (tskIDLE_PRIORITY + 1U)
#define TASK_STACK             512U
#define H_START_DELAY_MS       2000U
#define M_WORK_MS              12000U
#define LOGGER_DELAY_MS        14000U
#define TRACE_CAPACITY         80U

typedef struct {
    char code;
    TickType_t tick;
} TraceEvent;

static SemaphoreHandle_t xUartMutex = NULL;
static TaskHandle_t xHHandle = NULL;
static TaskHandle_t xMHandle = NULL;
static TaskHandle_t xLoggerHandle = NULL;
static volatile TraceEvent g_events[TRACE_CAPACITY];
static volatile uint32_t g_event_count = 0U;
static volatile uint32_t g_hook_calls = 0U;
static volatile uint32_t g_recording = 1U;

/* Only call from Tasks (not from ISR or scheduler hook). */
static void Trace_Event(char code)
{
    taskENTER_CRITICAL();
    if ((g_recording != 0U) && (g_event_count < TRACE_CAPACITY)) {
        uint32_t i = g_event_count;
        g_events[i].code = code;
        g_events[i].tick = xTaskGetTickCount();
        g_event_count = i + 1U;
    }
    taskEXIT_CRITICAL();
}

/*
 * Called by FreeRTOS tasks.c at task selection when enabled
 * through FreeRTOSConfig.h. Never print, block, or enter a
 * critical section in this hook.
 */
void Trace_OnSwitchIn(void *tcb)
{
    ++g_hook_calls;
    if ((g_recording == 0U) || (g_event_count >= TRACE_CAPACITY)) {
        return;
    }
    char code;
    if (tcb == (void *)xHHandle) {
        code = 'H';
    } else if (tcb == (void *)xMHandle) {
        code = 'M';
    } else {
        return; /* Idle and Logger are intentionally omitted */
    }
    uint32_t i = g_event_count;
    g_events[i].code = code;
    g_events[i].tick = xTaskGetTickCount();
    g_event_count = i + 1U;
}

static void Clock_Init(void)
{
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {}
    RCC->CFGR &= ~(RCC_CFGR_SW | RCC_CFGR_HPRE |
                   RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}
    SystemCoreClockUpdate();
    configASSERT(SystemCoreClock == 16000000UL);
}

static void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    GPIOB->MODER &= ~((3UL << 0) | (3UL << 14));
    GPIOB->MODER |=   (1UL << 0) | (1UL << 14);
    GPIOB->OTYPER &= ~((1UL << 0) | (1UL << 7));
    GPIOB->BSRR = (1UL << 16) | (1UL << 23);
}
static void LD1_ON(void)  { GPIOB->BSRR = (1UL << 0); }
static void LD1_OFF(void) { GPIOB->BSRR = (1UL << 16); }
static void LD2_ON(void)  { GPIOB->BSRR = (1UL << 7); }
static void LD2_OFF(void) { GPIOB->BSRR = (1UL << 23); }

static void UART3_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    GPIOD->MODER &= ~(3UL << 16);
    GPIOD->MODER |= (2UL << 16);
    GPIOD->OTYPER &= ~(1UL << 8);
    GPIOD->AFR[1] &= ~(0xFUL << 0);
    GPIOD->AFR[1] |= (7UL << 0);
    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;
    USART3->BRR = 139U;   /* 16MHz / 115200, OVER8=0 */
    USART3->CR1 = USART_CR1_UE | USART_CR1_TE;
}
static void UART3_Char(char c)
{
    while ((USART3->SR & USART_SR_TXE) == 0U) {}
    USART3->DR = (uint8_t)c;
}
static void UART3_String(const char *s)
{
    while (*s) UART3_Char(*s++);
}
static void UART3_U32(uint32_t value)
{
    char digits[10];
    uint32_t n = 0U;
    do {
        digits[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    while (n) UART3_Char(digits[--n]);
}

static void Task_H(void *argument)
{
    (void)argument;
    /* H initially blocks so that M acquires the UART mutex. */
    vTaskDelay(pdMS_TO_TICKS(H_START_DELAY_MS));

    /* When this point executes, H was selected to run.
     * NO artificial 3-second busy loop here.
     */
    LD1_ON();
    Trace_Event('T'); /* H is about to request mutex */

    /* Mutex is owned by M -> H blocks. */
    if (xSemaphoreTake(xUartMutex, portMAX_DELAY) == pdTRUE) {
        Trace_Event('A');  /* H woke up and acquired mutex */
        UART3_String("[H] Mutex acquired successfully\r\n");
        LD1_OFF();
        xSemaphoreGive(xUartMutex);
    }
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000U));
}

static void Task_M(void *argument)
{
    (void)argument;
    if (xSemaphoreTake(xUartMutex, portMAX_DELAY) == pdTRUE) {
        LD2_ON();
        Trace_Event('K');
        UART3_String("[M] Mutex acquired successfully\r\n");
        UART3_String("[M] Shared resource operation started\r\n");

        /* 12 seconds ELAPSED (not 12 seconds accumulated CPU time).
         * M will be interrupted naturally when H's delay expires.
         */
        TickType_t start = xTaskGetTickCount();
        while ((TickType_t)(xTaskGetTickCount() - start)
               < pdMS_TO_TICKS(M_WORK_MS)) {
            __NOP();
        }
        UART3_String("[M] Shared resource operation completed\r\n");
        LD2_OFF();
        Trace_Event('G');
        xSemaphoreGive(xUartMutex);
    }
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000U));
}

static void Trace_Print(void)
{
    UART3_String("\r\n================================\r\n");
    UART3_String(" FreeRTOS ACTUAL TASK TRACE\r\n");
    UART3_String("================================\r\n");
    UART3_String("Trace hook calls: ");
    UART3_U32(g_hook_calls);
    UART3_String("\r\n");
    if (g_hook_calls == 0U) {
        UART3_String("ERROR: traceTASK_SWITCHED_IN not enabled in FreeRTOSConfig.h\r\n");
    }
    for (uint32_t i = 0U; i < g_event_count; ++i) {
        UART3_Char('[');
        UART3_U32((uint32_t)g_events[i].tick);
        UART3_String(" ticks] ");
        switch (g_events[i].code) {
            case 'H': UART3_String("SWITCH IN: H\r\n"); break;
            case 'M': UART3_String("SWITCH IN: M\r\n"); break;
            case 'K': UART3_String("M: Mutex acquired\r\n"); break;
            case 'T': UART3_String("H: Mutex TAKE requested\r\n"); break;
            case 'G': UART3_String("M: Mutex GIVE requested\r\n"); break;
            case 'A': UART3_String("H: Mutex TAKE succeeded\r\n"); break;
            default: break;
        }
    }
    UART3_String("================================\r\n");
    UART3_String("END OF TRACE\r\n");
}

static void Task_Logger(void *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(LOGGER_DELAY_MS));
    taskENTER_CRITICAL();
    g_recording = 0U;
    taskEXIT_CRITICAL();
    if (xSemaphoreTake(xUartMutex, portMAX_DELAY) == pdTRUE) {
        Trace_Print();
        xSemaphoreGive(xUartMutex);
    }
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000U));
}

static void Create_Objects(void)
{
    BaseType_t ok;
    xUartMutex = xSemaphoreCreateMutex();
    configASSERT(xUartMutex != NULL);
    if (xUartMutex == NULL) for (;;) {}

    ok = xTaskCreate(Task_H, "H", TASK_STACK, NULL, H_PRIORITY, &xHHandle);
    configASSERT(ok == pdPASS);
    if (ok != pdPASS) for (;;) {}

    ok = xTaskCreate(Task_M, "M", TASK_STACK, NULL, M_PRIORITY, &xMHandle);
    configASSERT(ok == pdPASS);
    if (ok != pdPASS) for (;;) {}

    ok = xTaskCreate(Task_Logger, "LOG", TASK_STACK, NULL,
                     LOGGER_PRIORITY, &xLoggerHandle);
    configASSERT(ok == pdPASS);
    if (ok != pdPASS) for (;;) {}
}

int main(void)
{
    Clock_Init();
    GPIO_Init();
    UART3_Init();
    Create_Objects();
    vTaskStartScheduler();
    for (;;) {}
}


/*
config.h
/* ==================================================
 * Task Switch Trace Hook
 * ================================================== */

#ifdef __cplusplus
extern "C" {
#endif

void Trace_OnSwitchIn(void *tcb);

#ifdef __cplusplus
}
#endif

#ifdef traceTASK_SWITCHED_IN
#undef traceTASK_SWITCHED_IN
#endif

#define traceTASK_SWITCHED_IN() \
    Trace_OnSwitchIn((void *)pxCurrentTCB)

*/