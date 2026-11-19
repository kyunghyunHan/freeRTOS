
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
// #include "uart_dma_demo.h"
void UART_DMA_DemoStart(void);

/*
 * STM32F429ZI
 *
 * USART3 + DMA1 + FreeRTOS
 * Circular Ring Buffer + Zero-Copy
 *
 * USART3 TX : PD8 (AF7)
 * USART3 RX : PD9 (AF7)
 *
 * USART3 Clock : PCLK1 = 16MHz
 * Baudrate     : 115200, 8N1
 *
 * DMA RX:
 * DMA1 Stream1 Channel4
 *
 * Buffer: 512 Bytes
 * Half  : 256 Bytes
 *
 * DMA Half Transfer / Transfer Complete
 *          |
 *          v
 *     Task Notification
 *          |
 *          v
 *      RX Task
 *          |
 *          v
 *   Zero-Copy Processing
 *
 * Monitor Task:
 * 1초마다 UART 통계 출력
 *
 * HAL UART/DMA API 사용하지 않음.
 * FreeRTOS Kernel은 기존 프로젝트 사용.
 */

// ============================================
// 1. Configuration
// ============================================

#define UART_BAUDRATE 115200UL
#define UART_PCLK     16000000UL

#define RX_BUFFER_SIZE 512U
#define RX_HALF_SIZE   (RX_BUFFER_SIZE / 2U)

#define RX_TASK_STACK       512U
#define MONITOR_TASK_STACK  512U

#define RX_TASK_PRIORITY      (tskIDLE_PRIORITY + 3)
#define MONITOR_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

// FreeRTOS FromISR API 사용 가능 우선순위
// configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
// 보다 숫자가 같거나 커야 한다.
#define DMA_IRQ_PRIORITY 6U

// DMA1 Stream1 LISR Flags
#define DMA_FEIF1  (1UL << 6)
#define DMA_DMEIF1 (1UL << 8)
#define DMA_TEIF1  (1UL << 9)
#define DMA_HTIF1  (1UL << 10)
#define DMA_TCIF1  (1UL << 11)

#define DMA_ALL_FLAGS \
    (DMA_FEIF1 | DMA_DMEIF1 | DMA_TEIF1 | \
     DMA_HTIF1 | DMA_TCIF1)

// DMA1 Stream1 CR Bits
#define DMA_EN     (1UL << 0)
#define DMA_DMEIE  (1UL << 1)
#define DMA_TEIE   (1UL << 2)
#define DMA_HTIE   (1UL << 3)
#define DMA_TCIE   (1UL << 4)
#define DMA_CIRC   (1UL << 8)
#define DMA_MINC   (1UL << 10)

// ============================================
// 2. DMA Buffer
// ============================================

// DMA가 직접 데이터를 기록하는 RAM
static uint8_t rx_dma_buffer[RX_BUFFER_SIZE]
    __attribute__((aligned(4)));

// DMA 완료된 Half Block 개수
volatile uint32_t g_dma_completed_halves = 0;

// 실제 처리한 바이트 수
volatile uint32_t g_rx_bytes = 0;

// 처리된 256 Byte Block 개수
volatile uint32_t g_rx_blocks = 0;

// 처리 불가능했던 Half Block 개수
volatile uint32_t g_dropped_halves = 0;

// DMA 오류
volatile uint32_t g_dma_errors = 0;

// Checksum (32-bit rolling sum)
volatile uint32_t g_checksum = 0;

// 마지막 처리 바이트
volatile uint32_t g_last_byte = 0;

// DMA 동작 준비 완료
volatile uint32_t g_dma_ready = 0;

// Task Handle
static TaskHandle_t rx_task_handle = NULL;

// ============================================
// 3. UART TX Functions
// ============================================

static void UART3_SendChar(char c)
{
    // TXE = 1
    while ((USART3->SR & (1UL << 7)) == 0U)
    {
    }

    USART3->DR = (uint8_t)c;
}

static void UART3_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART3_SendChar(*str++);
    }
}

static void UART3_SendU32(uint32_t value)
{
    char buf[11];
    uint32_t i = 0;

    if (value == 0U)
    {
        UART3_SendChar('0');
        return;
    }

    while (value > 0U)
    {
        buf[i++] = '0' + (value % 10U);
        value /= 10U;
    }

    while (i > 0U)
    {
        UART3_SendChar(buf[--i]);
    }
}

static void UART3_SendHex(uint32_t value)
{
    const char hex[] = "0123456789ABCDEF";

    UART3_SendString("0x");

    for (int i = 7; i >= 0; i--)
    {
        uint32_t nibble =
            (value >> (i * 4)) & 0xFUL;

        UART3_SendChar(hex[nibble]);
    }
}

// ============================================
// 4. GPIO + USART3 Initialization
// ============================================

static void UART3_Init(void)
{
    // Enable GPIO D Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    // Enable USART3 Clock
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    // PD8 = TX, PD9 = RX
    // Alternate Function Mode
    GPIOD->MODER &= ~(
        (3UL << (8U * 2U)) |
        (3UL << (9U * 2U))
    );

    GPIOD->MODER |= (
        (2UL << (8U * 2U)) |
        (2UL << (9U * 2U))
    );

    // AF7 = USART3
    GPIOD->AFR[1] &= ~(
        (0xFUL << 0) |
        (0xFUL << 4)
    );

    GPIOD->AFR[1] |= (
        (7UL << 0) |
        (7UL << 4)
    );

    // RX Pull-Up
    GPIOD->PUPDR &= ~(3UL << (9U * 2U));
    GPIOD->PUPDR |=  (1UL << (9U * 2U));

    // Disable USART while configuring
    USART3->CR1 = 0;
    USART3->CR2 = 0;
    USART3->CR3 = 0;

    // OVER8 = 0
    // BRR approximately 0x008B
    USART3->BRR =
        (UART_PCLK + UART_BAUDRATE / 2U) /
        UART_BAUDRATE;

    // UE + TE + RE
    USART3->CR1 =
        USART_CR1_UE |
        USART_CR1_TE |
        USART_CR1_RE;
}

// ============================================
// 5. DMA1 Stream1 Initialization
// ============================================

static void DMA1_USART3_RX_Init(void)
{
    // Enable DMA1 Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    // Disable Stream1
    DMA1_Stream1->CR &= ~DMA_EN;

    // Wait for hardware to clear EN
    while (DMA1_Stream1->CR & DMA_EN)
    {
    }

    // Clear previous interrupt flags
    DMA1->LIFCR = DMA_ALL_FLAGS;

    // UART Data Register
    DMA1_Stream1->PAR =
        (uint32_t)(uintptr_t)&USART3->DR;

    // Memory Address
    DMA1_Stream1->M0AR =
        (uint32_t)(uintptr_t)rx_dma_buffer;

    // Number of Data Items
    DMA1_Stream1->NDTR = RX_BUFFER_SIZE;

    /*
     * Channel 4
     * Memory Increment Enable
     * Circular Mode
     * Peripheral to Memory
     * Byte Width
     * Half Transfer Interrupt
     * Transfer Complete Interrupt
     * Transfer Error Interrupt
     *
     * PL = 01 (Medium Priority)
     */

    DMA1_Stream1->CR =
        (4UL << 25) |
        (1UL << 16) |
        DMA_MINC |
        DMA_CIRC |
        DMA_HTIE |
        DMA_TCIE |
        DMA_TEIE |
        DMA_DMEIE;

    // Direct Mode
    DMA1_Stream1->FCR = 0;

    // DMA IRQ Priority
    NVIC_SetPriority(
        DMA1_Stream1_IRQn,
        DMA_IRQ_PRIORITY
    );

    NVIC_ClearPendingIRQ(DMA1_Stream1_IRQn);
    NVIC_EnableIRQ(DMA1_Stream1_IRQn);

    // USART3 DMA Receive Enable
    USART3->CR3 |= USART_CR3_DMAR;

    // Start DMA
    DMA1_Stream1->CR |= DMA_EN;

    g_dma_ready = 1;
}

// ============================================
// 6. DMA RX Interrupt Handler
// ============================================

void DMA1_Stream1_IRQHandler(void)
{
    BaseType_t higher_priority_woken = pdFALSE;

    // Read DMA status
    uint32_t status = DMA1->LISR;

    // DMA Error
    if (status & (DMA_FEIF1 |
                  DMA_DMEIF1 |
                  DMA_TEIF1))
    {
        g_dma_errors++;

        // Clear Error Flags
        DMA1->LIFCR =
            status & (DMA_FEIF1 |
                      DMA_DMEIF1 |
                      DMA_TEIF1);
    }

    // Half Transfer
    if (status & DMA_HTIF1)
    {
        DMA1->LIFCR = DMA_HTIF1;

        g_dma_completed_halves++;

        vTaskNotifyGiveFromISR(
            rx_task_handle,
            &higher_priority_woken
        );
    }

    // Transfer Complete
    if (status & DMA_TCIF1)
    {
        DMA1->LIFCR = DMA_TCIF1;

        g_dma_completed_halves++;

        vTaskNotifyGiveFromISR(
            rx_task_handle,
            &higher_priority_woken
        );
    }

    // Run higher-priority RX Task if required
    portYIELD_FROM_ISR(higher_priority_woken);
}

// ============================================
// 7. Zero-Copy Data Processing
// ============================================

static void Process_ZeroCopy(
    const uint8_t *data,
    uint32_t length)
{
    /*
     * data는 rx_dma_buffer 내부 주소.
     *
     * 별도 배열 복사 없음.
     * memcpy() 사용하지 않음.
     */

    uint32_t sum = g_checksum;

    for (uint32_t i = 0; i < length; i++)
    {
        uint8_t value = data[i];

        // Data Processing
        sum += value;

        // Last Byte
        g_last_byte = value;
    }

    g_checksum = sum;
    g_rx_bytes += length;
    g_rx_blocks++;
}

// ============================================
// 8. FreeRTOS RX Task
// ============================================

static void RX_Task(void *argument)
{
    (void)argument;

    // Number of completed halves already handled
    uint32_t consumed = 0;

    UART3_Init();
    DMA1_USART3_RX_Init();

    for (;;)
    {
        // Wait for DMA Interrupt
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );

        // Process completed DMA halves
        for (;;)
        {
            uint32_t produced =
                g_dma_completed_halves;

            // Nothing new
            if (produced == consumed)
            {
                break;
            }

            /*
             * DMA Ring Buffer = 2 halves.
             *
             * 두 개 이상 뒤처졌다면 가장 오래된
             * Half는 DMA가 다시 쓰고 있을 수 있음.
             *
             * 최신 완료 Half만 처리.
             */

            uint32_t backlog = produced - consumed;

            if (backlog > 1U)
            {
                g_dropped_halves += backlog - 1U;
                consumed = produced - 1U;
            }

            // Even: First Half
            // Odd : Second Half
            uint32_t offset =
                (consumed & 1U) ?
                RX_HALF_SIZE : 0U;

            // Zero-Copy Processing
            Process_ZeroCopy(
                &rx_dma_buffer[offset],
                RX_HALF_SIZE
            );

            consumed++;
        }
    }
}

// ============================================
// 9. UART Monitoring Task
// ============================================

static void MONITOR_Task(void *argument)
{
    (void)argument;

    while (!g_dma_ready)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    UART3_SendString(
        "\r\n"
        "================================\r\n"
        " STM32F429ZI UART DMA DEMO\r\n"
        " DMA Circular + Zero Copy\r\n"
        " FreeRTOS RX Task\r\n"
        "================================\r\n"
    );

    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(1000)
        );

        uint32_t dma_position =
            RX_BUFFER_SIZE - DMA1_Stream1->NDTR;

        UART3_SendString("\r\n");
        UART3_SendString("===== DMA MONITOR =====\r\n");

        UART3_SendString("Tick          : ");
        UART3_SendU32(xTaskGetTickCount());
        UART3_SendString("\r\n");

        UART3_SendString("RX Bytes      : ");
        UART3_SendU32(g_rx_bytes);
        UART3_SendString("\r\n");

        UART3_SendString("RX Blocks     : ");
        UART3_SendU32(g_rx_blocks);
        UART3_SendString("\r\n");

        UART3_SendString("DMA Halves    : ");
        UART3_SendU32(g_dma_completed_halves);
        UART3_SendString("\r\n");

        UART3_SendString("Dropped Halves: ");
        UART3_SendU32(g_dropped_halves);
        UART3_SendString("\r\n");

        UART3_SendString("DMA Errors    : ");
        UART3_SendU32(g_dma_errors);
        UART3_SendString("\r\n");

        UART3_SendString("DMA Position  : ");
        UART3_SendU32(dma_position);
        UART3_SendString("\r\n");

        UART3_SendString("DMA NDTR      : ");
        UART3_SendU32(DMA1_Stream1->NDTR);
        UART3_SendString("\r\n");

        UART3_SendString("Buffer Addr   : ");
        UART3_SendHex(
            (uint32_t)(uintptr_t)rx_dma_buffer
        );
        UART3_SendString("\r\n");

        UART3_SendString("Last Byte     : ");
        UART3_SendHex(g_last_byte);
        UART3_SendString("\r\n");

        UART3_SendString("Checksum      : ");
        UART3_SendHex(g_checksum);
        UART3_SendString("\r\n");

        UART3_SendString("=======================\r\n");
    }
}

// ============================================
// 10. Task Creation
// ============================================

void UART_DMA_DemoStart(void)
{
    BaseType_t result;

    result = xTaskCreate(
        RX_Task,
        "DMA_RX",
        RX_TASK_STACK,
        NULL,
        RX_TASK_PRIORITY,
        &rx_task_handle
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        MONITOR_Task,
        "DMA_MONITOR",
        MONITOR_TASK_STACK,
        NULL,
        MONITOR_TASK_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);
}
