
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*
 * ============================================================
 * STM32F429ZI - FreeRTOS Message Queue
 * ============================================================
 *
 * [실습 목적]
 *
 * FreeRTOS의 Message Queue를 이용하여
 * Producer Task와 Consumer Task 사이에서
 * 메시지를 전달하는 과정을 확인한다.
 *
 * [하드웨어]
 *
 * MCU        : STM32F429ZI
 * USART3 TX  : PD8 (AF7)
 * USART3 RX  : PD9 (AF7)
 * Baudrate   : 115200, 8N1
 * Clock      : HSI 16MHz
 *
 * HAL을 사용하지 않고 레지스터를 직접 제어한다.
 *
 * [FreeRTOS 구성]
 *
 * Producer Task : Priority 2
 * Consumer Task : Priority 3
 *
 * Message Queue Length : 5
 * Message Size         : 12 Bytes
 *
 * [전체 실행 순서]
 *
 * 1. MCU Reset / Startup
 * 2. main() 진입
 * 3. Clock_Init()
 * 4. UART3_Init()
 * 5. RTOS_Objects_Init()
 *      |
 *      +-- xQueueCreate()
 *      |
 *      +-- xTaskCreate(Producer)
 *      |
 *      +-- xTaskCreate(Consumer)
 *
 * 6. vTaskStartScheduler()
 *
 * 7. Consumer Task 먼저 실행
 *      |
 *      +-- UART 제목 출력
 *      |
 *      +-- xQueueReceive()
 *      |
 *      +-- Queue가 비어 있으므로 Blocked
 *
 * 8. Producer Task 실행
 *      |
 *      +-- Message 생성
 *      |
 *      +-- xQueueSend()
 *      |
 *      +-- Message Queue에 데이터 복사
 *
 * 9. Consumer Task가 Ready 상태로 변경
 *
 * 10. Scheduler가 Consumer 선택
 *
 * 11. Context Switching
 *
 * 12. Consumer Task 실행
 *      |
 *      +-- xQueueReceive() 완료
 *      |
 *      +-- 메시지를 지역 변수로 복사
 *      |
 *      +-- UART로 메시지 출력
 *
 * 13. Consumer는 다음 메시지를 기다림
 *
 * 14. Producer는 500ms 주기로 반복
 *
 * [주의]
 *
 * FreeRTOS Kernel / Startup / Linker Script는
 * 기존 프로젝트에 포함되어 있어야 한다.
 *
 * 기존 CubeMX에서 생성한 main(), Scheduler,
 * Interrupt Handler와 중복되지 않도록 한다.
 *
 * ============================================================
 */


// ============================================================
// 1. Configuration
// ============================================================

/*
 * USART3 Baudrate
 *
 * 115200bps
 *
 * UART에서 1초 동안 전송할 수 있는 Bit 수를 의미한다.
 */
#define UART_BAUDRATE 115200UL

/*
 * USART3 Peripheral Clock
 *
 * APB1 Clock = 16MHz 가정
 *
 * USART3는 APB1 Bus에 연결되어 있다.
 */
#define UART_PCLK 16000000UL

/*
 * Message Queue Length
 *
 * Queue에 저장할 수 있는 최대 메시지 개수.
 *
 * 현재 설정:
 *
 * [1] [2] [3] [4] [5]
 *
 * 최대 5개의 Message_t를 저장한다.
 */
#define QUEUE_LENGTH 5U

/*
 * Task Priority
 *
 * FreeRTOS는 일반적으로 숫자가 클수록
 * Task 우선순위가 높다.
 *
 * Consumer Priority = 3
 * Producer Priority = 2
 *
 * 따라서 두 Task가 모두 Ready 상태이면
 * Consumer가 먼저 실행된다.
 */
#define PRODUCER_PRIORITY (tskIDLE_PRIORITY + 2)
#define CONSUMER_PRIORITY (tskIDLE_PRIORITY + 3)

/*
 * Task Stack Size
 *
 * FreeRTOS xTaskCreate()의 Stack Depth는
 * Byte가 아니라 StackType_t 단위다.
 *
 * Cortex-M4에서는 일반적으로
 * StackType_t = 4Byte이므로
 *
 * 512 * 4Byte = 2048Byte
 *
 * Task마다 별도의 Stack이 할당된다.
 */
#define TASK_STACK_SIZE 512U


// ============================================================
// 2. Message Structure
// ============================================================

/*
 * Queue를 통해 전달할 메시지 구조체.
 *
 * id    : 메시지 번호
 * value : 메시지 데이터
 * tick  : 메시지를 생성한 RTOS Tick
 *
 * uint32_t = 4Byte
 *
 * 총 크기:
 *
 * 4 + 4 + 4 = 12Byte
 *
 * Queue는 메시지 하나를 저장할 때
 * 이 구조체 전체를 복사한다.
 *
 * Queue Length = 5
 *
 * 데이터 저장 공간:
 *
 * 12Byte * 5 = 60Byte
 *
 * Queue 관리 구조체의 메모리는 별도다.
 */
typedef struct
{
    uint32_t id;

    uint32_t value;

    uint32_t tick;

} Message_t;


// ============================================================
// 3. Global Variables
// ============================================================

/*
 * Queue Handle
 *
 * xQueueCreate()가 반환하는 Handle.
 *
 * Producer와 Consumer가 동일한 Queue를
 * 사용하도록 연결하는 역할을 한다.
 *
 * NULL은 아직 Queue가 생성되지 않았다는 의미.
 */
static QueueHandle_t xMessageQueue = NULL;

/*
 * 통계 변수
 *
 * g_sent_count:
 *   Queue 전송 성공 횟수
 *
 * g_received_count:
 *   Queue 수신 성공 횟수
 *
 * g_send_fail_count:
 *   Queue 전송 실패 횟수
 *
 * volatile은 컴파일러의 접근 최적화를 제한하지만
 * Atomic 연산이나 Task 동기화를 보장하지 않는다.
 *
 * 현재 실습에서는 각 통계 변수를 주로 하나의
 * Task가 수정하며 Consumer가 상태를 출력한다.
 */
static volatile uint32_t g_sent_count = 0;

static volatile uint32_t g_received_count = 0;

static volatile uint32_t g_send_fail_count = 0;


// ============================================================
// 4. System Clock Initialization
// ============================================================

static void Clock_Init(void)
{
    /*
     * [1] HSI Enable
     *
     * RCC_CR의 HSION Bit를 1로 설정한다.
     *
     * HSI = High Speed Internal Oscillator
     *
     * STM32F429ZI의 HSI는 16MHz다.
     */
    RCC->CR |= RCC_CR_HSION;

    /*
     * [2] HSI Ready 확인
     *
     * HSIRDY = 1이 될 때까지 대기한다.
     *
     * 클럭이 안정화되기 전에 다음 설정을
     * 진행하지 않기 위한 과정이다.
     */
    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    /*
     * [3] Clock Prescaler 설정
     *
     * System Clock = HSI 16MHz
     *
     * AHB Prescaler  = 1
     * APB1 Prescaler = 1
     * APB2 Prescaler = 1
     *
     * 결과:
     *
     * SYSCLK = 16MHz
     * HCLK   = 16MHz
     * PCLK1  = 16MHz
     * PCLK2  = 16MHz
     *
     * RCC_CFGR_SW = 00
     *   System Clock Source = HSI
     *
     * HPRE = 0000
     *   AHB Prescaler = 1
     *
     * PPRE1 = 000
     *   APB1 Prescaler = 1
     *
     * PPRE2 = 000
     *   APB2 Prescaler = 1
     */
    RCC->CFGR &= ~(
        RCC_CFGR_SW |
        RCC_CFGR_HPRE |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );

    /*
     * [4] System Clock Source 확인
     *
     * SWS = 00이면 HSI가 SYSCLK로
     * 사용되고 있다는 의미다.
     */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
    }

    /*
     * [5] SystemCoreClock 갱신
     *
     * CMSIS에서 관리하는 SystemCoreClock 변수를
     * 실제 RCC 설정을 기준으로 갱신한다.
     */
    SystemCoreClockUpdate();

    /*
     * [6] Clock 검증
     *
     * 실제 SystemCoreClock이 16MHz가 아니면
     * Assertion이 발생하도록 한다.
     */
    configASSERT(SystemCoreClock == 16000000UL);

    /*
     * [7] NVIC Priority Grouping
     *
     * Cortex-M4의 NVIC Priority 설정.
     *
     * Grouping 3은 CMSIS 인코딩 기준으로
     * STM32F429ZI의 4개 구현 Priority Bit를
     * 모두 Preemption Priority에 사용한다.
     *
     * FreeRTOS의 Interrupt Priority 설정과
     * 호환되도록 구성한다.
     */
    NVIC_SetPriorityGrouping(3U);
}


// ============================================================
// 5. USART3 Initialization
// ============================================================

static void UART3_Init(void)
{
    /*
     * [1] GPIOD Clock Enable
     *
     * USART3 TX = PD8
     * USART3 RX = PD9
     *
     * GPIO를 사용하기 전에 RCC에서
     * GPIOD Clock을 활성화해야 한다.
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    /*
     * [2] USART3 Clock Enable
     *
     * USART3는 APB1 Peripheral이다.
     */
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    /*
     * [3] PD8 / PD9 Mode 설정
     *
     * GPIO MODER:
     *
     * 00 = Input
     * 01 = General Output
     * 10 = Alternate Function
     * 11 = Analog
     *
     * PD8 = Alternate Function
     * PD9 = Alternate Function
     *
     * 먼저 해당 Bit Field를 0으로 초기화한다.
     */
    GPIOD->MODER &= ~(
        (3UL << 16) |
        (3UL << 18)
    );

    /*
     * MODER8 = 10
     * MODER9 = 10
     */
    GPIOD->MODER |= (
        (2UL << 16) |
        (2UL << 18)
    );

    /*
     * [4] Output Type
     *
     * OTYPER = 0
     *
     * Push-Pull 방식으로 설정한다.
     */
    GPIOD->OTYPER &= ~(
        (1UL << 8) |
        (1UL << 9)
    );

    /*
     * [5] Alternate Function 설정
     *
     * STM32F429ZI
     *
     * PD8 = USART3_TX = AF7
     * PD9 = USART3_RX = AF7
     *
     * AFR[1]은 GPIO Pin 8~15의
     * Alternate Function을 설정한다.
     *
     * 각 Pin은 4Bit를 사용한다.
     *
     * PD8  = AFRH Bit 3:0
     * PD9  = AFRH Bit 7:4
     */

    // 기존 Alternate Function 값 제거
    GPIOD->AFR[1] &= ~(
        (0xFUL << 0) |
        (0xFUL << 4)
    );

    // AF7 설정
    GPIOD->AFR[1] |= (
        (7UL << 0) |
        (7UL << 4)
    );

    /*
     * [6] RX Pull-Up
     *
     * UART RX는 Idle 상태에서 HIGH다.
     *
     * PD9에 내부 Pull-Up을 설정한다.
     */
    GPIOD->PUPDR &= ~(3UL << 18);

    GPIOD->PUPDR |= (1UL << 18);

    /*
     * [7] USART3 Disable
     *
     * 설정을 변경하는 동안 USART를
     * 비활성화한다.
     *
     * CR1 = Control Register 1
     * CR2 = Control Register 2
     * CR3 = Control Register 3
     */
    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;

    /*
     * [8] Baudrate 설정
     *
     * PCLK1 = 16MHz
     * Baudrate = 115200
     *
     * Oversampling by 16
     *
     * BRR = 16000000 / 115200
     *     ≈ 138.89
     *
     * 반올림하면 139 = 0x008B
     */
    USART3->BRR =
        (UART_PCLK + UART_BAUDRATE / 2U) /
        UART_BAUDRATE;

    /*
     * [9] USART Enable
     *
     * UE = USART Enable
     * TE = Transmitter Enable
     * RE = Receiver Enable
     *
     * 8 Data Bits
     * No Parity
     * 1 Stop Bit
     */
    USART3->CR1 =
        USART_CR1_UE |
        USART_CR1_TE |
        USART_CR1_RE;
}


// ============================================================
// 6. UART TX Functions
// ============================================================

/*
 * UART3_SendChar()
 *
 * 문자 1개를 USART3로 송신한다.
 *
 * 실행 과정:
 *
 * 1. USART3 SR의 TXE 확인
 * 2. TXE = 1이 될 때까지 대기
 * 3. DR에 문자 저장
 * 4. USART 하드웨어가 문자 송신
 *
 * TXE = Transmit Data Register Empty
 */
static void UART3_SendChar(char c)
{
    while ((USART3->SR & USART_SR_TXE) == 0U)
    {
    }

    USART3->DR = (uint8_t)c;
}

/*
 * UART3_SendString()
 *
 * 문자열을 문자 단위로 송신한다.
 *
 * 문자열 끝의 NULL 문자('\0')를
 * 만날 때까지 반복한다.
 */
static void UART3_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART3_SendChar(*str++);
    }
}

/*
 * UART3_SendU32()
 *
 * uint32_t 정수를 ASCII 문자열 형태로
 * USART3로 출력한다.
 *
 * 예:
 *
 * value = 123
 *
 * UART 출력:
 *
 * '1' '2' '3'
 *
 * 내부 과정:
 *
 * 1. 10으로 나눈 나머지 추출
 * 2. ASCII 문자 변환
 * 3. Buffer에 역순 저장
 * 4. 역순으로 출력하여 정상 숫자 표현
 */
static void UART3_SendU32(uint32_t value)
{
    char buffer[11];
    uint32_t index = 0U;

    if (value == 0U)
    {
        UART3_SendChar('0');
        return;
    }

    // 숫자를 자릿수별로 분리
    while (value > 0U)
    {
        buffer[index++] =
            (char)('0' + (value % 10U));

        value /= 10U;
    }

    // 역순 출력
    while (index > 0U)
    {
        UART3_SendChar(buffer[--index]);
    }
}


// ============================================================
// 7. Producer Task
// ============================================================

/*
 * Producer Task
 *
 * 역할:
 *
 * 1. 새로운 Message 생성
 * 2. Message ID 증가
 * 3. Message Value 생성
 * 4. 현재 RTOS Tick 기록
 * 5. Queue에 Message 전송
 * 6. 500ms 주기 대기
 *
 * Priority = 2
 *
 * Consumer Priority가 3이므로
 * Consumer가 Ready 상태라면
 * Consumer가 먼저 실행될 수 있다.
 */

static void Producer_Task(void *argument)
{
    (void)argument;

    /*
     * [1] Producer 지역 변수
     *
     * Message_t는 구조체다.
     *
     * message는 Producer Task의 Stack에
     * 위치할 수 있다.
     *
     * 컴파일러 최적화에 따라 실제 배치는 달라진다.
     */
    Message_t message;

    /*
     * 메시지 번호
     *
     * 최초 값 = 0
     */
    uint32_t message_id = 0U;

    /*
     * [2] 최초 Wake-up 기준 Tick 저장
     *
     * xTaskGetTickCount():
     * 현재 FreeRTOS Tick Count 반환
     *
     * vTaskDelayUntil()은 이 값을 기준으로
     * 주기적인 Task 실행 시점을 관리한다.
     */
    TickType_t last_wake = xTaskGetTickCount();

    /*
     * [3] 무한 반복
     *
     * FreeRTOS Task 함수는 일반적으로
     * 무한 반복문 안에서 작업을 수행한다.
     */
    for (;;)
    {
        /*
         * [4] Message ID 증가
         *
         * 최초:
         *
         * message_id = 0
         *
         * 증가 후:
         *
         * message_id = 1
         */
        message_id++;

        /*
         * [5] Message 구조체에 데이터 저장
         *
         * 첫 번째 메시지:
         *
         * id    = 1
         * value = 10
         * tick  = 현재 Tick
         *
         * 두 번째 메시지:
         *
         * id    = 2
         * value = 20
         * tick  = 현재 Tick
         */
        message.id = message_id;

        message.value = message_id * 10U;

        message.tick = xTaskGetTickCount();

        /*
         * [6] xQueueSend()
         *
         * 현재 메시지를 Queue 내부 메모리에 복사한다.
         *
         * 매개변수:
         *
         * xMessageQueue
         *   → 데이터를 보낼 Queue Handle
         *
         * &message
         *   → 복사할 Message_t의 주소
         *
         * pdMS_TO_TICKS(100)
         *   → Queue가 가득 차면 최대 100ms 대기
         *
         * 중요:
         *
         * Queue는 &message 주소만 저장하는 것이 아니라
         * sizeof(Message_t)만큼 데이터를 복사한다.
         *
         * 따라서 Producer의 message가 바뀌어도
         * Queue에 저장된 기존 메시지는 유지된다.
         *
         * Queue가 비어 있고 Consumer가
         * xQueueReceive()로 기다리고 있었다면,
         * 메시지 전송으로 Consumer가 Ready 상태가 된다.
         *
         * Consumer Priority = 3
         * Producer Priority = 2
         *
         * 선점형 스케줄링에서는 Consumer가
         * Producer를 선점할 수 있다.
         */
        BaseType_t result = xQueueSend(
            xMessageQueue,
            &message,
            pdMS_TO_TICKS(100)
        );

        /*
         * [7] Queue 전송 결과 확인
         *
         * pdPASS:
         *   Queue 전송 성공
         *
         * pdFAIL:
         *   Queue가 가득 차고 Timeout까지
         *   공간이 생기지 않아 전송 실패
         */
        if (result == pdPASS)
        {
            g_sent_count++;
        }
        else
        {
            g_send_fail_count++;
        }

        /*
         * [8] vTaskDelayUntil()
         *
         * Producer를 500ms 주기로 실행하도록 한다.
         *
         * 예:
         *
         * 0ms    → Message 1
         * 500ms  → Message 2
         * 1000ms → Message 3
         * 1500ms → Message 4
         *
         * 호출 후 다음 기준 시각이 아직
         * 도달하지 않았다면:
         *
         * Running → Blocked
         *
         * 기준 시각에 도달하면:
         *
         * Blocked → Ready
         *
         * Scheduler가 선택하면:
         *
         * Ready → Running
         *
         * 작업 실행 시간이 주기보다 길면
         * Blocked 상태에 들어가지 않을 수도 있다.
         */
        vTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(500)
        );
    }
}


// ============================================================
// 8. Consumer Task
// ============================================================

/*
 * Consumer Task
 *
 * 역할:
 *
 * 1. Queue에서 메시지가 들어오기를 기다림
 * 2. 메시지를 수신
 * 3. Message_t 구조체 복사
 * 4. USART3로 메시지 내용 출력
 * 5. 다시 Queue 수신 대기
 *
 * Priority = 3
 *
 * Producer보다 높은 우선순위.
 *
 * Queue가 비어 있다면 Blocked 상태로 대기한다.
 */

static void Consumer_Task(void *argument)
{
    (void)argument;

    /*
     * [1] 수신 메시지 저장 변수
     *
     * Queue에서 수신한 Message_t를
     * 저장할 지역 변수다.
     */
    Message_t received;

    /*
     * [2] 최초 실행 시 프로그램 제목 출력
     *
     * Consumer Task는 Producer보다
     * 우선순위가 높다.
     *
     * 따라서 Scheduler 시작 시 다른
     * 더 높은 우선순위 Task가 없다면
     * Consumer가 먼저 실행된다.
     */
    UART3_SendString(
        "\r\n"
        "================================\r\n"
        " STM32F429ZI FreeRTOS Queue\r\n"
        " Producer -> Queue -> Consumer\r\n"
        "================================\r\n"
    );

    for (;;)
    {
        /*
         * [3] xQueueReceive()
         *
         * Queue에서 메시지를 수신한다.
         *
         * xMessageQueue
         *   → 수신할 Queue
         *
         * &received
         *   → 데이터를 복사할 메모리 주소
         *
         * portMAX_DELAY
         *   → 설정에 따라 무기한 대기
         *
         * Queue에 메시지가 있으면:
         *
         * 1. 가장 앞의 메시지 확인
         * 2. received에 구조체 데이터 복사
         * 3. Queue에서 메시지 제거
         * 4. Queue Count 감소
         * 5. pdPASS 반환
         *
         * Queue가 비어 있으면:
         *
         * 1. Task를 Queue 수신 대기 리스트에 등록
         * 2. Consumer Running → Blocked
         * 3. Scheduler가 다른 Ready Task 선택
         *
         * Producer가 Queue에 메시지를 넣으면:
         *
         * 1. Consumer Blocked → Ready
         * 2. Scheduler가 실행 Task 선택
         * 3. Consumer가 다시 Running
         * 4. 메시지 수신 완료 후 함수 반환
         */
        BaseType_t result = xQueueReceive(
            xMessageQueue,
            &received,
            portMAX_DELAY
        );

        /*
         * [4] 메시지 수신 성공 확인
         *
         * pdPASS이면 received 구조체에
         * Queue에서 받은 데이터가 저장된다.
         */
        if (result == pdPASS)
        {
            /*
             * [5] 수신 성공 횟수 증가
             */
            g_received_count++;

            /*
             * [6] Message 출력 시작
             */
            UART3_SendString(
                "\r\n[QUEUE RECEIVE]\r\n"
            );

            /*
             * [7] Message ID 출력
             *
             * Producer가 만든 메시지 번호.
             *
             * FIFO 방식이므로 일반적인
             * xQueueSend() / xQueueReceive()에서는
             * 먼저 저장된 메시지부터 수신한다.
             */
            UART3_SendString("Message ID : ");
            UART3_SendU32(received.id);
            UART3_SendString("\r\n");

            /*
             * [8] Message Value 출력
             *
             * value = id * 10
             */
            UART3_SendString("Value      : ");
            UART3_SendU32(received.value);
            UART3_SendString("\r\n");

            /*
             * [9] Send Tick 출력
             *
             * Producer가 메시지를 생성할 때
             * 기록한 Tick Count.
             */
            UART3_SendString("Send Tick  : ");
            UART3_SendU32(received.tick);
            UART3_SendString("\r\n");

            /*
             * [10] Receive Tick 출력
             *
             * Consumer가 현재 실행 중인
             * 시점의 RTOS Tick Count.
             *
             * Send Tick과 비교하면 메시지가
             * 생성된 이후 현재 출력 시점까지
             * 경과한 Tick을 살펴볼 수 있다.
             *
             * 엄밀한 Queue Latency 측정은
             * 수신 직후 Tick을 별도로 기록해야 한다.
             */
            UART3_SendString("Recv Tick  : ");
            UART3_SendU32(xTaskGetTickCount());
            UART3_SendString("\r\n");

            /*
             * [11] Queue Count 확인
             *
             * Queue에 현재 남아 있는
             * 메시지 개수를 반환한다.
             *
             * 예:
             *
             * Queue Count = 0
             *   → 메시지 없음
             *
             * Queue Count = 3
             *   → 메시지 3개 저장 중
             *
             * Queue Count = 5
             *   → Queue 가득 참
             */
            UART3_SendString("Queue Count: ");

            UART3_SendU32(
                uxQueueMessagesWaiting(xMessageQueue)
            );

            UART3_SendString("\r\n");

            /*
             * [12] Queue 송신 성공 횟수 출력
             */
            UART3_SendString("Sent       : ");
            UART3_SendU32(g_sent_count);
            UART3_SendString("\r\n");

            /*
             * [13] Queue 수신 성공 횟수 출력
             */
            UART3_SendString("Received   : ");
            UART3_SendU32(g_received_count);
            UART3_SendString("\r\n");

            /*
             * [14] Queue 송신 실패 횟수 출력
             *
             * Producer가 Queue에 메시지를
             * 보내지 못한 횟수다.
             */
            UART3_SendString("Send Failed: ");
            UART3_SendU32(g_send_fail_count);
            UART3_SendString("\r\n");

            /*
             * [15] 메시지 처리 완료
             *
             * for(;;) 처음으로 돌아간다.
             *
             * 다음 xQueueReceive() 호출 시
             * Queue가 비어 있으면 다시 Blocked 된다.
             *
             * 따라서 Consumer가 계속 CPU를
             * 점유하며 Queue를 검사하지 않는다.
             */
        }
    }
}


// ============================================================
// 9. Queue + Task Creation
// ============================================================

static void RTOS_Objects_Init(void)
{
    BaseType_t result;

    /*
     * [1] Message Queue 생성
     *
     * xQueueCreate(
     *     Queue Length,
     *     Item Size
     * );
     *
     * Queue Length = 5
     * Item Size = sizeof(Message_t) = 12Byte
     *
     * FreeRTOS Kernel은 Queue 관리 구조체와
     * 메시지 저장 공간을 할당한다.
     *
     * 생성된 Queue는 처음에 비어 있다.
     *
     * Queue Count = 0
     */
    xMessageQueue = xQueueCreate(
        QUEUE_LENGTH,
        sizeof(Message_t)
    );

    /*
     * [2] Queue 생성 성공 확인
     *
     * NULL이면 생성 실패.
     *
     * Heap 부족 등의 원인으로 실패할 수 있다.
     */
    configASSERT(xMessageQueue != NULL);

    if (xMessageQueue == NULL)
    {
        for (;;)
        {
        }
    }

    /*
     * [3] Producer Task 생성
     *
     * xTaskCreate() 실행 과정:
     *
     * 1. TCB 메모리 할당
     * 2. Task Stack 메모리 할당
     * 3. 초기 Stack Context 구성
     * 4. Task 진입 함수 주소 설정
     * 5. Task Priority 설정
     * 6. Ready List에 등록
     *
     * Scheduler 시작 전이므로
     * Producer_Task() 함수가 아직
     * 실행되는 것은 아니다.
     */
    result = xTaskCreate(
        Producer_Task,
        "PRODUCER",
        TASK_STACK_SIZE,
        NULL,
        PRODUCER_PRIORITY,
        NULL
    );

    /*
     * Task 생성 결과 확인
     */
    configASSERT(result == pdPASS);

    if (result != pdPASS)
    {
        for (;;)
        {
        }
    }

    /*
     * [4] Consumer Task 생성
     *
     * Producer와 동일하게
     * TCB와 Stack을 생성한다.
     *
     * Consumer Priority = 3
     * Producer Priority = 2
     *
     * Consumer가 더 높은 우선순위다.
     */
    result = xTaskCreate(
        Consumer_Task,
        "CONSUMER",
        TASK_STACK_SIZE,
        NULL,
        CONSUMER_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);

    if (result != pdPASS)
    {
        for (;;)
        {
        }
    }

    /*
     * [5] RTOS 객체 생성 완료
     *
     * 현재 상태:
     *
     * Message Queue 생성 완료
     *
     * Producer Task = Ready
     * Consumer Task = Ready
     *
     * Scheduler는 아직 시작하지 않았다.
     */
}


// ============================================================
// 10. Main
// ============================================================

int main(void)
{
    /*
     * [1] Startup 완료 후 main 진입
     *
     * 실제로는 다음 과정이 먼저 진행된다.
     *
     * Reset
     *   |
     *   v
     * Vector Table
     *   |
     *   v
     * Reset_Handler
     *   |
     *   v
     * .data 초기화
     *   |
     *   v
     * .bss 초기화
     *   |
     *   v
     * main()
     */

    /*
     * [2] System Clock 초기화
     */
    Clock_Init();

    /*
     * [3] USART3 초기화
     *
     * GPIOD Clock Enable
     * USART3 Clock Enable
     * PD8 / PD9 AF7
     * Baudrate 설정
     * USART Enable
     */
    UART3_Init();

    /*
     * [4] FreeRTOS 객체 생성
     *
     * Queue 생성
     * Producer Task 생성
     * Consumer Task 생성
     */
    RTOS_Objects_Init();

    /*
     * [5] Scheduler 시작
     *
     * FreeRTOS Kernel은 Scheduler를 시작하고
     * Ready Task 중 실행할 Task를 선택한다.
     *
     * 현재:
     *
     * Consumer Priority = 3
     * Producer Priority = 2
     *
     * 따라서 일반적으로 Consumer가 먼저 실행된다.
     *
     * Consumer가 Queue Receive에서 Blocked 되면
     * Producer가 실행된다.
     *
     * 정상적으로 Scheduler가 시작되면
     * 일반적으로 이 함수는 반환되지 않는다.
     */
    vTaskStartScheduler();

    /*
     * [6] Scheduler 시작 실패 시 도달 가능
     *
     * 정상적으로 Scheduler가 실행되면
     * 아래 반복문은 실행되지 않는다.
     */
    for (;;)
    {
    }
}
