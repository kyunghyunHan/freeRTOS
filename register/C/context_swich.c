
#include <stdint.h>

/*
 * STM32F429ZI - Cortex-M4
 *
 * PendSV Context Switching 직접 구현
 *
 * HAL / FreeRTOS 미사용
 * HSI = 16MHz, SYSCLK = 16MHz 가정
 *
 * Task A : PB0 LED 500ms Toggle
 * Task B : PB7 LED 200ms Toggle
 *
 * SysTick : 1ms
 * PendSV  : Context Switching
 * SVC     : First Task Start
 *
 * 주의:
 * FPU Context Saving은 구현하지 않음.
 * 학습용 최소 스케줄러.
 */

// ============================================
// 1. Register Address
// ============================================

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_AHB1ENR  REG32(0x40023830UL)

#define GPIOB_MODER  REG32(0x40020400UL)
#define GPIOB_OTYPER REG32(0x40020404UL)
#define GPIOB_BSRR   REG32(0x40020418UL)

#define SYST_CSR     REG32(0xE000E010UL)
#define SYST_RVR     REG32(0xE000E014UL)
#define SYST_CVR     REG32(0xE000E018UL)

#define SCB_ICSR     REG32(0xE000ED04UL)
#define SCB_SHPR3    REG32(0xE000ED20UL)

#define PENDSVSET    (1UL << 28)

// ============================================
// 2. Task Configuration
// ============================================

#define TASK_COUNT       2U
#define TASK_STACK_WORDS 256U

typedef struct
{
    // Assembly에서 offset 0으로 접근
    uint32_t *sp;

    // GDB에서 확인하기 위한 정보
    uint32_t id;
    uint32_t switches;

} TCB_t;

// 각 Task의 Stack
static uint32_t taskA_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t taskB_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

// TCB 배열
TCB_t tasks[TASK_COUNT];

// 현재 실행 중인 Task
TCB_t * volatile current_task = 0;

// 스케줄러 인덱스
volatile uint32_t current_index = 0;

// Tick 및 Switch 횟수
volatile uint32_t g_ms = 0;
volatile uint32_t g_switch_count = 0;

// Task 실행 확인용
volatile uint32_t taskA_count = 0;
volatile uint32_t taskB_count = 0;

// ============================================
// 3. GPIO Initialization
// ============================================

static void GPIO_Init(void)
{
    // GPIOB Clock Enable
    RCC_AHB1ENR |= (1UL << 1);

    // PB0, PB7 Output
    GPIOB_MODER &= ~((3UL << 0) | (3UL << 14));
    GPIOB_MODER |=  ((1UL << 0) | (1UL << 14));

    // Push-Pull
    GPIOB_OTYPER &= ~((1UL << 0) | (1UL << 7));

    // LED OFF
    GPIOB_BSRR = (1UL << 16) | (1UL << 23);
}

// ============================================
// 4. SysTick Initialization
// ============================================

static void SysTick_Init(void)
{
    // PendSV Priority = 15
    // SysTick Priority = 14
    //
    // STM32F429 Cortex-M4:
    // 숫자가 클수록 낮은 예외 우선순위

    SCB_SHPR3 =
        (SCB_SHPR3 & 0x0000FFFFUL)
        | (0xF0UL << 16)
        | (0xE0UL << 24);

    // 16MHz / 1000 = 16000
    SYST_RVR = 16000UL - 1UL;

    // Current Value Reset
    SYST_CVR = 0;

    // ENABLE | TICKINT | CLKSOURCE
    SYST_CSR = 0x07UL;
}

// ============================================
// 5. Task Function
// ============================================

void TaskA(void)
{
    uint32_t last_tick;

    // 첫 Task가 시작되면 SysTick 활성화
    SysTick_Init();

    last_tick = g_ms;

    while (1)
    {
        if ((uint32_t)(g_ms - last_tick) >= 500U)
        {
            last_tick += 500U;

            // PB0 Toggle
            static uint32_t led_state = 0;

            led_state ^= 1U;

            if (led_state)
                GPIOB_BSRR = (1UL << 0);
            else
                GPIOB_BSRR = (1UL << 16);

            taskA_count++;
        }
    }
}

void TaskB(void)
{
    uint32_t last_tick = g_ms;

    while (1)
    {
        if ((uint32_t)(g_ms - last_tick) >= 200U)
        {
            last_tick += 200U;

            // PB7 Toggle
            static uint32_t led_state = 0;

            led_state ^= 1U;

            if (led_state)
                GPIOB_BSRR = (1UL << 7);
            else
                GPIOB_BSRR = (1UL << 23);

            taskB_count++;
        }
    }
}

// ============================================
// 6. Task Exit Handler
// ============================================

void TaskExit(void)
{
    // Task가 실수로 return 하면 여기서 정지
    while (1)
    {
        __asm volatile ("bkpt #0");
    }
}

// ============================================
// 7. Task Stack Initialization
// ============================================

static void TaskStack_Init(
    TCB_t *tcb,
    uint32_t *stack,
    void (*entry)(void),
    uint32_t id)
{
    uint32_t *sp = &stack[TASK_STACK_WORDS];

    /*
     * Cortex-M Exception Stack Frame
     *
     * Hardware Restored:
     *
     * xPSR
     * PC
     * LR
     * R12
     * R3
     * R2
     * R1
     * R0
     *
     * Software Restored:
     *
     * R11 ~ R4
     */

    // Hardware Exception Frame
    *(--sp) = 0x01000000UL;       // xPSR (Thumb)
    *(--sp) = (uint32_t)entry;    // PC
    *(--sp) = (uint32_t)TaskExit; // LR
    *(--sp) = 0;                  // R12
    *(--sp) = 0;                  // R3
    *(--sp) = 0;                  // R2
    *(--sp) = 0;                  // R1
    *(--sp) = 0;                  // R0

    // Software Context: R4 ~ R11
    for (uint32_t i = 0; i < 8U; i++)
    {
        *(--sp) = 0;
    }

    tcb->sp = sp;
    tcb->id = id;
    tcb->switches = 0;
}

// ============================================
// 8. Scheduler
// ============================================

__attribute__((noinline))
void Scheduler_SelectNext(void)
{
    // 현재 Task의 Switch-out 횟수
    current_task->switches++;

    // Round Robin
    current_index++;

    if (current_index >= TASK_COUNT)
    {
        current_index = 0;
    }

    // 다음 TCB 선택
    current_task = &tasks[current_index];

    g_switch_count++;
}

// ============================================
// 9. SysTick Interrupt
// ============================================

void SysTick_Handler(void)
{
    // System Tick 증가
    g_ms++;

    // PendSV Pending 설정
    // 실제 Context Switching은 PendSV에서 실행
    SCB_ICSR = PENDSVSET;
}

// ============================================
// 10. Main
// ============================================

int main(void)
{
    GPIO_Init();

    // Task A Stack 생성
    TaskStack_Init(
        &tasks[0],
        taskA_stack,
        TaskA,
        0
    );

    // Task B Stack 생성
    TaskStack_Init(
        &tasks[1],
        taskB_stack,
        TaskB,
        1
    );

    current_index = 0;
    current_task = &tasks[0];

    // SVC로 첫 번째 Task 시작
    __asm volatile ("svc #0" ::: "memory");

    // 정상적으로는 도달하지 않음
    __builtin_unreachable();
}
