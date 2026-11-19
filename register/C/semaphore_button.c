
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*
 * =====================================================
 * STM32F429ZI - FreeRTOS Binary Semaphore
 * =====================================================
 *
 * HAL 사용 안 함
 *
 * B1 Button : PC13 (Active-High)
 * LD1 LED   : PB0  (Active-High)
 *
 * 동작:
 *
 * 1. main()에서 Binary Semaphore 생성
 * 2. LED Task 생성
 * 3. Scheduler 시작
 * 4. LED Task가 Semaphore Take 호출
 * 5. 신호가 없으면 LED Task는 Blocked
 * 6. B1 버튼 누름
 * 7. EXTI13 인터럽트 발생
 * 8. ISR에서 Semaphore Give
 * 9. LED Task가 Ready 상태로 전환
 * 10. Scheduler가 선택하면 LED Task Running
 * 11. LED Toggle
 * 12. 다시 Semaphore Take 대기
 *
 *
 * ① B1 버튼(PC13) 누름
          ↓
② EXTI13이 Rising Edge 감지
          ↓
③ NVIC가 인터럽트 요청 처리
          ↓
④ EXTI15_10_IRQHandler() 실행
          ↓
⑤ EXTI13 Pending Flag 확인 및 해제
          ↓
⑥ xSemaphoreGiveFromISR()
   → Semaphore에 신호 전달
          ↓
⑦ LED Task가 Blocked → Ready
          ↓
⑧ Scheduler가 LED Task 선택
          ↓
⑨ xSemaphoreTake() 성공
          ↓
⑩ LED_Toggle() 실행
 *
 * FreeRTOS Kernel / Startup / Linker는
 * 기존 프로젝트에 포함되어 있어야 한다.
 * =====================================================
 */

// =====================================================
// 1. Configuration
// =====================================================

#define LED_PIN 0U
#define BUTTON_PIN 13U

#define LED_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define LED_TASK_STACK_SIZE 256U

#define BUTTON_DEBOUNCE_MS 50U

// =====================================================
// 2. Global Semaphore Handle
// =====================================================

/*
 * Semaphore Handle
 *
 * main()에서 생성한 Semaphore를
 * ISR과 LED Task가 공유한다.
 *
 * Binary Semaphore:
 *
 * Count = 0 : 신호 없음
 * Count = 1 : 신호 있음
 *
 * 최초 생성 시 Count = 0
 */
static SemaphoreHandle_t xButtonSemaphore = NULL;

// =====================================================
// 3. GPIO Initialization
// =====================================================

static void GPIO_Init(void)
{
    /*
     * [1] GPIOB / GPIOC Clock Enable
     *
     * GPIOB : LD1 LED
     * GPIOC : B1 Button
     */
    RCC->AHB1ENR |=
        RCC_AHB1ENR_GPIOBEN |
        RCC_AHB1ENR_GPIOCEN;

    (void)RCC->AHB1ENR;

    /*
     * [2] PB0 = Output
     *
     * MODER0 = 01
     */
    GPIOB->MODER &= ~(3UL << (LED_PIN * 2U));
    GPIOB->MODER |= (1UL << (LED_PIN * 2U));

    // Push-Pull
    GPIOB->OTYPER &= ~(1UL << LED_PIN);

    // 초기 LED OFF
    GPIOB->BSRR = (1UL << (LED_PIN + 16U));

    /*
     * [3] PC13 = Input
     *
     * MODER13 = 00
     */
    GPIOC->MODER &= ~(3UL << (BUTTON_PIN * 2U));

    /*
     * NUCLEO-F429ZI B1 버튼 회로의
     * 외부 Pull-Down을 사용한다.
     *
     * 내부 Pull-Up / Pull-Down 비활성화
     */
    GPIOC->PUPDR &= ~(3UL << (BUTTON_PIN * 2U));
}

// =====================================================
// 4. EXTI13 Initialization
// =====================================================

static void EXTI13_Init(void)
{
    /*
     * [1] SYSCFG Clock Enable
     *
     * EXTI13 입력 소스를 GPIOC로
     * 연결하기 위해 SYSCFG가 필요하다.
     */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    (void)RCC->APB2ENR;

    /*
     * [2] EXTI13 Interrupt Mask
     *
     * 초기화 중 인터럽트 차단
     */
    EXTI->IMR &= ~(1UL << 13);

    /*
     * [3] EXTI13 = PC13
     *
     * EXTICR4의 EXTI13 필드:
     *
     * 0000 = PA13
     * 0001 = PB13
     * 0010 = PC13
     */
    SYSCFG->EXTICR[3] &= ~(0xFUL << 4);
    SYSCFG->EXTICR[3] |= (0x2UL << 4);

    /*
     * [4] Rising Edge Trigger
     *
     * NUCLEO-F429ZI B1:
     *
     * 평소  : LOW
     * 누름  : HIGH
     *
     * 버튼을 누르는 순간
     * LOW -> HIGH 변화 감지
     */
    EXTI->RTSR |= (1UL << 13);

    // Falling Edge 비활성화
    EXTI->FTSR &= ~(1UL << 13);

    /*
     * [5] 기존 Pending Flag 제거
     *
     * PR 레지스터는 W1C 방식.
     *
     * 1을 쓰면 해당 Pending Bit가
     * 클리어된다.
     */
    EXTI->PR = (1UL << 13);

    /*
     * [6] NVIC Priority Grouping
     *
     * STM32F429ZI의 구현된 Priority Bit를
     * Preemption Priority로 사용한다.
     */
    NVIC_SetPriorityGrouping(3U);

    /*
     * [7] EXTI15_10 IRQ Priority
     *
     * ISR에서 FreeRTOS API를 호출하므로
     * configMAX_SYSCALL_INTERRUPT_PRIORITY
     * 제한을 지켜야 한다.
     *
     * Cortex-M4 FreeRTOS 설정값은
     * 일반적으로 왼쪽 정렬된 값이므로,
     * CMSIS NVIC_SetPriority()가 받는
     * Priority 숫자로 변환한다.
     */
    NVIC_SetPriority(
        EXTI15_10_IRQn,
        (uint32_t)configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS));

    /*
     * [8] EXTI13 Interrupt 허용
     */
    EXTI->IMR |= (1UL << 13);

    /*
     * [9] NVIC에서 EXTI15_10 IRQ 활성화
     */
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

// =====================================================
// 5. LED Toggle
// =====================================================

static void LED_Toggle(void)
{
    /*
     * GPIOB ODR의 PB0 상태 확인
     *
     * LED가 ON이면 OFF
     * LED가 OFF이면 ON
     *
     * BSRR을 이용하여 Set / Reset한다.
     */
    if ((GPIOB->ODR & (1UL << LED_PIN)) != 0U)
    {
        // LED OFF
        GPIOB->BSRR = (1UL << (LED_PIN + 16U));
    }
    else
    {
        // LED ON
        GPIOB->BSRR = (1UL << LED_PIN);
    }
}

// =====================================================
// 6. LED Task
// =====================================================

static void LED_Task(void *argument)
{
    (void)argument;

    /*
     * 버튼 접점 바운싱 방지를 위한
     * 마지막 정상 입력 시각
     */
    TickType_t last_press = 0U;
    BaseType_t first_press = pdTRUE;

    for (;;)
    {
        /*
         * [1] Semaphore Take
         *
         * xButtonSemaphore의 신호를 기다린다.
         *
         * 최초 Count = 0
         *
         * 신호가 없으면:
         *
         * LED Task
         *
         * Running -> Blocked
         *
         * 이때 CPU는 다른 Task를 실행한다.
         *
         * 버튼 ISR에서 Give가 발생하면:
         *
         * Blocked -> Ready
         *
         * Scheduler가 선택하면:
         *
         * Ready -> Running
         *
         * Take 성공 시 Semaphore 신호를
         * 획득하고 pdTRUE를 반환한다.
         */
        if (xSemaphoreTake(
                xButtonSemaphore,
                portMAX_DELAY) == pdTRUE)
        {
            /*
             * [2] 버튼 이벤트 수신 성공
             *
             * 여기까지 실행되었다는 것은
             * Semaphore를 획득했다는 의미다.
             *
             * 실제 버튼 데이터가 전달된 것은
             * 아니다.
             *
             * 신호를 받았다는 사실만 전달된다.
             */

            TickType_t now = xTaskGetTickCount();

            /*
             * [3] Software Debouncing
             *
             * 버튼을 한 번 눌렀을 때
             * 접점 진동으로 여러 인터럽트가
             * 발생할 수 있다.
             *
             * 마지막 유효 입력 이후
             * 50ms 이내의 이벤트는 무시한다.
             *
             * 첫 번째 버튼 입력은
             * 무조건 처리한다.
             */
            if (first_press == pdTRUE ||
                (TickType_t)(now - last_press) >=
                    pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS))
            {
                first_press = pdFALSE;
                last_press = now;

                /*
                 * [4] LED 상태 변경
                 *
                 * 첫 번째 버튼:
                 *   OFF -> ON
                 *
                 * 두 번째 버튼:
                 *   ON -> OFF
                 *
                 * 이후 반복
                 */
                LED_Toggle();
            }

            /*
             * [5] 다시 for(;;) 처음으로 이동
             *
             * 다음 xSemaphoreTake() 호출.
             *
             * Count = 0이면 다시 Blocked.
             *
             * Count = 1이면 바로 Take 성공.
             */
        }
    }
}

// =====================================================
// 7. EXTI15_10 Interrupt Handler
// =====================================================

void EXTI15_10_IRQHandler(void)
{
    /*
     * ISR에서 FreeRTOS API를 호출할 때
     * 필요한 변수.
     *
     * Give에 의해 더 높은 우선순위의
     * Task가 깨어났는지 기록한다.
     */
    BaseType_t higher_priority_woken = pdFALSE;

    /*
     * [1] EXTI13 Pending 확인
     *
     * PC13에서 Rising Edge가 발생하면
     * EXTI13 Pending Bit가 설정된다.
     */
    if ((EXTI->PR & (1UL << 13)) != 0U)
    {
        /*
         * [2] Pending Flag Clear
         *
         * W1C: 1을 써서 클리어
         */
        EXTI->PR = (1UL << 13);

        /*
         * [3] Semaphore Give
         *
         * 이 부분이 실제로
         * Semaphore 신호를 보내는 곳이다.
         *
         * ISR이 Binary Semaphore에
         * 신호를 전달한다.
         *
         * LED Task가 Take에서 기다리고
         * 있었다면 Ready 상태가 된다.
         *
         * ISR에서는 일반 xSemaphoreGive()
         * 대신 FromISR 버전을 사용한다.
         */
        if (xButtonSemaphore != NULL)
        {
            xSemaphoreGiveFromISR(
                xButtonSemaphore,
                &higher_priority_woken);
        }
    }

    /*
     * [4] Context Switching 요청
     *
     * 더 높은 우선순위 Task가
     * 깨어났다면 ISR 종료 시점에
     * Scheduler가 실행 Task를
     * 다시 선택하도록 요청한다.
     */
    portYIELD_FROM_ISR(higher_priority_woken);
}

// =====================================================
// 8. FreeRTOS Objects Initialization
// =====================================================

static void RTOS_Objects_Init(void)
{
    /*
     * [1] Binary Semaphore 생성
     *
     * 내부 초기 Count = 0
     *
     * 이 시점에는 아직 신호가 없다.
     */
    xButtonSemaphore = xSemaphoreCreateBinary();

    configASSERT(xButtonSemaphore != NULL);

    if (xButtonSemaphore == NULL)
    {
        for (;;)
        {
        }
    }

    /*
     * [2] LED Task 생성
     *
     * Task 함수  : LED_Task
     * Task 이름  : LED
     * Stack      : 256 Words
     * Priority   : 2
     *
     * xTaskCreate()는 Task의
     * TCB와 Stack을 생성한다.
     *
     * 아직 Scheduler를 시작하지 않았으므로
     * Task 함수는 실행되지 않는다.
     */
    BaseType_t result = xTaskCreate(
        LED_Task,
        "LED",
        LED_TASK_STACK_SIZE,
        NULL,
        LED_TASK_PRIORITY,
        NULL);

    configASSERT(result == pdPASS);

    if (result != pdPASS)
    {
        for (;;)
        {
        }
    }
}

// =====================================================
// 9. Main
// =====================================================

int main(void)
{
    /*
     * [1] GPIO 초기화
     *
     * PB0 : LED Output
     * PC13: Button Input
     */
    GPIO_Init();

    /*
     * [2] Semaphore / Task 생성
     *
     * EXTI 인터럽트를 활성화하기 전에
     * Semaphore를 먼저 생성한다.
     */
    RTOS_Objects_Init();

    /*
     * [3] EXTI13 설정
     *
     * PC13 -> EXTI13 -> NVIC
     */
    EXTI13_Init();

    /*
     * [4] Scheduler 시작
     *
     * LED Task 실행
     *
     * LED Task는 xSemaphoreTake()를 호출하고
     * 신호가 없으면 Blocked 상태가 된다.
     *
     * 버튼을 누르면 ISR이 Semaphore Give.
     *
     * 이후 LED Task가 실행되어
     * LED를 Toggle한다.
     */
    vTaskStartScheduler();

    /*
     * 정상 실행 시 도달하지 않는다.
     */
    for (;;)
    {
    }
}
