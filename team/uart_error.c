#include "ultrasonic.h"
#include "stm32f4xx.h"

/* 측정 상태 */
#define US_IDLE       0U   /* 아무것도 안 함 */
#define US_WAIT_RISE  1U   /* TRIG 보냄, ECHO 상승 에지 기다리는 중 */
#define US_WAIT_FALL  2U   /* ECHO High 중, 하강 에지 기다리는 중 */
#define US_DONE       3U   /* 측정 완료 */

#define US_TIMEOUT_US 40000U   /* HC-SR04 는 물체 없으면 최대 약 38 ms High */

static volatile uint8_t  us_state   = US_IDLE;
static volatile uint32_t us_rise    = 0U;
static volatile uint32_t us_echo_us = 0U;

static void delay_us(uint32_t us)
{
    uint32_t start = TIM5->CNT;
    while ((TIM5->CNT - start) < us) {
    }
}

void ULTRASONIC_Init(void)
{
    /* GPIOA, GPIOF, TIM5 클럭 공급 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOFEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    /* PF12 (TRIG): 일반 출력 모드(01), Push-pull, 초기 Low */
    GPIOF->MODER &= ~(3U << 24);
    GPIOF->MODER |=  (1U << 24);
    GPIOF->OTYPER &= ~(1U << 12);
    GPIOF->PUPDR &= ~(3U << 24);
    GPIOF->BSRR = (1U << (12 + 16));

    /* PA0 (ECHO): AF 모드(10), AF2 = TIM5_CH1 */
    GPIOA->MODER &= ~(3U << 0);
    GPIOA->MODER |=  (2U << 0);
    GPIOA->PUPDR &= ~(3U << 0);
    GPIOA->AFR[0] &= ~(0xFU << 0);
    GPIOA->AFR[0] |=  (2U << 0);

    /* 설정하는 동안 카운터와 인터럽트 정지 */
    TIM5->CR1  = 0U;
    TIM5->DIER = 0U;

    /* APB1 = 42 MHz, 분주 있으므로 타이머 클럭 = 84 MHz -> 84분주 = 1 MHz (1칸 = 1 us) */
    TIM5->PSC = 84U - 1U;
    TIM5->ARR = 0xFFFFFFFFU;           /* TIM5 는 32비트: 최대로 두고 계속 돌림 */

    /* CH1 입력 캡처: CC1S = 01 (TI1 입력), IC1F = 0011 (짧은 노이즈 필터) */
    TIM5->CCMR1 &= ~(0xFFU << 0);
    TIM5->CCMR1 |=  (1U << 0) | (3U << 4);

    /* CC1P = 1, CC1NP = 1 : 상승/하강 양쪽 에지 캡처, CC1E = 1 : 캡처 허용 */
    TIM5->CCER &= ~(0xFU << 0);
    TIM5->CCER |=  (1U << 1) | (1U << 3) | (1U << 0);

    /* UG: 분주값 적용 및 카운터 초기화 후 플래그 제거 */
    TIM5->EGR = (1U << 0);
    TIM5->SR  = 0U;

    /* CC1IE = 1 : 채널1 캡처 인터럽트 허용 */
    TIM5->DIER |= (1U << 1);

    NVIC_ClearPendingIRQ(TIM5_IRQn);
    NVIC_EnableIRQ(TIM5_IRQn);

    /* CEN: 카운터 시작 */
    TIM5->CR1 |= (1U << 0);
}

void ULTRASONIC_Start(void)
{
    us_state = US_WAIT_RISE;

    /* TRIG High 10 us 이상 -> Low */
    GPIOF->BSRR = (1U << 12);
    delay_us(12U);
    GPIOF->BSRR = (1U << (12 + 16));
}

uint8_t ULTRASONIC_IsDone(void)
{
    return (us_state == US_DONE) ? 1U : 0U;
}

uint32_t ULTRASONIC_GetEchoUs(void)
{
    return us_echo_us;
}

uint32_t ULTRASONIC_GetCm(void)
{
    /* 음속 340 m/s 기준 왕복: 거리(cm) = 시간(us) / 58 */
    return us_echo_us / 58U;
}

uint32_t ULTRASONIC_GetMm(void)
{
    return (us_echo_us * 10U) / 58U;
}

uint32_t ULTRASONIC_MeasureCm(void)
{
    ULTRASONIC_Start();

    uint32_t start = TIM5->CNT;
    while (us_state != US_DONE) {
        if ((TIM5->CNT - start) > US_TIMEOUT_US) {
            us_state = US_IDLE;
            return ULTRASONIC_ERROR;
        }
    }
    return ULTRASONIC_GetCm();
}

void TIM5_IRQHandler(void)
{
    /* 채널1 캡처 발생? */
    if ((TIM5->SR & (1U << 1)) != 0U)
    {
        /* CCR1 읽으면 CC1IF 자동 해제 */
        uint32_t cap = TIM5->CCR1;

        /* 지금 핀 레벨로 상승/하강 구분 */
        if ((GPIOA->IDR & (1U << 0)) != 0U)
        {
            if (us_state == US_WAIT_RISE) {
                us_rise  = cap;
                us_state = US_WAIT_FALL;
            }
        }
        else
        {
            if (us_state == US_WAIT_FALL) {
                us_echo_us = cap - us_rise;   /* 32비트라 넘침도 뺄셈으로 처리됨 */
                us_state   = US_DONE;
            }
        }
    }

    /* 오버캡처 플래그(CC1OF, bit9) 정리: SR 은 0 을 쓴 비트만 지워짐 */
    TIM5->SR = ~(1U << 9);
}
