
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>

/*
STM32F429ZI + FreeRTOS

B1     : PC13 (EXTI13, Rising Edge)
LD2    : PB7 (LED Toggle)
USART3 : PD8(TX), PD9(RX)
Baud   : 115200, 8N1

전체 실행 순서

① HAL / Clock / GPIO / UART 초기화
② Binary Semaphore 생성 (초기 0)
③ KeyTask 생성 → READY
④ FreeRTOS Scheduler 시작
⑤ KeyTask RUNNING / EXTI 활성화
⑥ xSemaphoreTake() 호출
⑦ Semaphore = 0 → KeyTask BLOCKED
⑧ PendSV ① → 다른 Task 또는 Idle 실행
⑨ B1 버튼 인터럽트 발생
⑩ CPU 하드웨어 Context 자동 저장
⑪ EXTI ISR 실행
⑫ xSemaphoreGiveFromISR()
⑬ portYIELD_FROM_ISR() → PendSV 요청
⑭ PendSV ② → KeyTask RUNNING
⑮ xSemaphoreTake() 성공 반환
⑯ 디바운싱 → LED Toggle → UART 출력
⑰ 다시 Semaphore Take 대기

위 흐름은 KeyTask보다 높은 우선순위의
다른 Ready Task가 없는 대표적인 상황이다.

디바운싱 과정의 vTaskDelay()로 인해
추가 Context Switching이 발생할 수 있다.
*/

UART_HandleTypeDef huart3;

SemaphoreHandle_t keySemaphore = NULL;

/*
함수 선언
*/

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void KeyTask(void *argument);
void Error_Handler(void);


/*
①~④ main()

시스템 초기화
Semaphore 생성
Task 생성
Scheduler 시작
*/

int main(void)
{
    /*
    ① HAL 초기화

    Reset_Handler와 C Runtime 초기화 이후
    main()에 진입한 상태이다.

    HAL_Init()
    - HAL 내부 초기화
    - HAL Tick 초기화
    - NVIC Priority Grouping 설정

    SystemClock_Config()
    - HSI 16MHz 사용

    MX_GPIO_Init()
    - PB7 LED 출력 설정
    - PC13 EXTI Rising Edge 설정
    - EXTI IRQ 우선순위 설정
    - IRQ는 아직 활성화하지 않음

    MX_USART3_UART_Init()
    - USART3 초기화
    - PD8 TX / PD9 RX
    - 115200 baud
    */

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();

    MX_USART3_UART_Init();


    /*
    ② Binary Semaphore 생성

    xSemaphoreCreateBinary()

    내부 Queue 기반 제어 구조체 생성

    uxLength = 1
    uxItemSize = 0
    uxMessagesWaiting = 0

    xTasksWaitingToSend 초기화
    xTasksWaitingToReceive 초기화

    Semaphore 초기 상태 = Empty (0)

    uxItemSize = 0이므로
    일반 Queue와 달리 데이터 항목을
    복사하기 위한 저장 공간은 필요하지 않다.

    Semaphore 생성에 성공하면
    SemaphoreHandle_t를 반환한다.
    */

    keySemaphore = xSemaphoreCreateBinary();

    /*
    Semaphore 생성 성공 여부 확인

    NULL이면 동적 메모리 할당 실패 등의
    문제가 발생한 것이다.
    */

    if (keySemaphore == NULL)
    {
        Error_Handler();
    }


    /*
    ③ KeyTask 생성

    xTaskCreate() 내부 과정

    1. TCB 생성
       Task Control Block

    2. Task Stack 메모리 할당

    3. 초기 CPU Context 구성

       PC   : KeyTask 함수 진입 주소
       xPSR : Thumb 상태 설정
       LR   : Task 종료 시 사용할 복귀 정보
       기타 레지스터 초기 Context 준비

    4. TCB에 초기 Stack Pointer 저장

    5. Task Priority = 2

    6. Ready List에 Task 등록

    결과

    KeyTask : READY

    아직 CPU 실행권을 확보한 상태는 아니다.

    Stack Depth = 256
    Cortex-M4 FreeRTOS 기준 256 StackType_t 단위
    일반적으로 1024 Byte이다.
    */

    if (xTaskCreate(
            KeyTask,
            "KeyTask",
            256,
            NULL,
            2,
            NULL) != pdPASS)
    {
        Error_Handler();
    }


    /*
    ④ FreeRTOS Scheduler 시작

    vTaskStartScheduler()

    내부 과정

    1. Idle Task 생성

    2. FreeRTOS Tick 설정

    3. Scheduler 실행 준비

    4. 실행할 Ready Task 선택

    5. 최초 Task의 CPU Context 복원

    6. Thread Mode에서 Task 실행 시작

    KeyTask Priority = 2
    Idle Task Priority = 0

    더 높은 우선순위의 Ready Task가 없다면
    KeyTask가 먼저 실행된다.

    첫 번째 Task 시작은
    일반적인 PendSV 기반 Task 간 전환과
    구별해야 한다.
    */

    vTaskStartScheduler();


    /*
    Scheduler가 정상적으로 시작되면
    일반적으로 이 위치로 돌아오지 않는다.
    */

    Error_Handler();

    while (1)
    {
    }
}


/*
⑤~⑧, ⑭~⑰ KeyTask

Thread Mode 실행
PSP 사용

Semaphore를 기다리다가
인터럽트로 신호를 받으면
LED와 UART 작업을 수행한다.
*/

static void KeyTask(void *argument)
{
    const uint8_t msg[] = "[key]\r\n";

    (void)argument;


    /*
    ⑤ KeyTask CPU 실행권 확보

    현재 상태

    KeyTask : READY → RUNNING

    CPU는 Thread Mode에서 실행한다.

    FreeRTOS Task는 PSP를 사용한다.

    KeyTask의 명령어가
    실제 CPU에서 실행되기 시작한다.
    */


    /*
    EXTI 인터럽트 활성화

    Scheduler 시작 이후 실행한다.

    Semaphore와 KeyTask가
    이미 생성된 상태이다.

    처리 순서

    1. EXTI13 Pending Flag Clear

    2. NVIC Pending Clear

    3. EXTI15_10_IRQn Enable

    이후 B1 버튼에 Rising Edge가 발생하면
    EXTI 인터럽트가 실행될 수 있다.
    */

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);

    HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);

    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);


    for (;;)
    {
        /*
        ⑥ xSemaphoreTake() 호출

        현재 KeyTask = RUNNING

        Semaphore 사용 가능 여부 확인

        YES : Semaphore = 1

        - Take 즉시 성공
        - Semaphore 신호 소비
        - Semaphore = 0
        - pdTRUE 반환
        - 아래 if 내부 실행

        NO : Semaphore = 0

        - 기다릴 수 있는 시간 확인
        - 이 코드는 portMAX_DELAY 사용
        - 무기한 대기 설정이라면 Blocked 진입


        ⑦ KeyTask RUNNING → BLOCKED

        Semaphore가 비어 있으므로
        Take는 아직 성공을 반환하지 않는다.

        FreeRTOS 내부에서

        1. KeyTask를 Semaphore 대기 List에 등록

        2. KeyTask를 Blocked 상태로 변경

        3. 다른 Task가 CPU를 사용할 수 있도록
           Context Switching 요청


        ⑧ 첫 번째 PendSV

        KeyTask가 Blocked되면
        다른 Ready Task를 실행해야 한다.

        PendSV가 실행되는 경우

        CPU 하드웨어 자동 저장

        R0~R3
        R12
        LR
        PC
        xPSR

        PendSV_Handler 소프트웨어 저장

        R4~R11 저장

        현재 PSP를
        KeyTask TCB에 저장

        vTaskSwitchContext()

        다음 Ready Task 선택

        다른 Ready Task가 없으면
        Idle Task 선택

        다음 Task의 PSP 읽기

        R4~R11 소프트웨어 복원

        Exception Return

        CPU가 기본 레지스터 자동 복원

        결과

        KeyTask : BLOCKED
        Idle Task : RUNNING

        여기까지 진행되면
        KeyTask는 Semaphore 신호를 기다린다.

        버튼 인터럽트가 발생한 뒤
        GiveFromISR()가 호출되면
        KeyTask가 다시 Ready가 될 수 있다.
        */

        if (xSemaphoreTake(
                keySemaphore,
                portMAX_DELAY) == pdTRUE)
        {
            /*
            ⑭~⑮ KeyTask 실행 재개

            ISR에서 Semaphore를 Give하면
            KeyTask가 Blocked에서 Ready로 변경된다.

            Scheduler가 KeyTask를 선택하면

            KeyTask : READY → RUNNING

            CPU 실행권 확보

            KeyTask의 CPU Context 복원 완료

            xSemaphoreTake() 실행 재개

            Semaphore 신호 소비

            pdTRUE 반환

            이후 if 내부 코드 실행

            주의

            Semaphore가 이미 1이었다면
            Blocked와 PendSV를 거치지 않고
            즉시 이 위치에 도달할 수 있다.
            */


            /*
            ⑯ 버튼 디바운싱 시작

            vTaskDelay(20ms)

            Task가 일정 시간 기다리면서
            버튼 입력이 안정되도록 한다.

            KeyTask : RUNNING → BLOCKED

            CPU 실행권을 다른 Task에 넘긴다.

            Tick 시간이 경과하면

            KeyTask : BLOCKED → READY

            Scheduler가 선택하면

            KeyTask : READY → RUNNING

            따라서 이 지연 과정에서도
            추가 Context Switching이 발생한다.
            */

            vTaskDelay(pdMS_TO_TICKS(20));


            /*
            버튼 입력 상태 확인

            PC13 = HIGH ?

            YES
            - 실제 버튼이 눌려 있는 것으로 판단
            - LED Toggle 및 UART 출력 수행

            NO
            - 안정화 후 눌린 상태가 아니므로
              이번 이벤트 처리 생략

            STM32F429ZI B1은
            눌렀을 때 HIGH가 되는 구성이다.
            */

            if (HAL_GPIO_ReadPin(
                    GPIOC,
                    GPIO_PIN_13) == GPIO_PIN_SET)
            {
                /*
                LD2 LED Toggle

                PB7 출력 상태를 반전한다.

                LOW → HIGH
                HIGH → LOW

                실제 LED 상태가 ON/OFF로 변경된다.

                LED 제어는 ISR이 아닌
                KeyTask의 Thread Mode에서 실행된다.
                */

                HAL_GPIO_TogglePin(
                    GPIOB,
                    GPIO_PIN_7);


                /*
                USART3 메시지 전송

                UART 출력 데이터

                [key]\r\n

                USART3 TX = PD8
                Baudrate = 115200

                HAL_UART_Transmit()는
                Blocking 방식 전송 함수이다.

                현재 CPU는 KeyTask를 실행하고 있다.

                ISR에서는 UART 전송을 수행하지 않는다.
                */

                HAL_UART_Transmit(
                    &huart3,
                    (uint8_t *)msg,
                    sizeof(msg) - 1,
                    100);


                /*
                버튼을 누르고 있는 동안
                추가 이벤트를 처리하지 않도록 대기한다.

                PC13 = HIGH ?

                YES
                - 버튼이 눌린 상태
                - vTaskDelay(10ms)
                - 다시 버튼 상태 확인

                NO
                - 버튼 해제 확인
                - while 반복문 종료

                각 vTaskDelay() 호출 시

                RUNNING → BLOCKED

                시간이 지나면

                BLOCKED → READY

                CPU 실행권을 다시 확보하면
                while 조건을 검사한다.
                */

                while (HAL_GPIO_ReadPin(
                           GPIOC,
                           GPIO_PIN_13) == GPIO_PIN_SET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }


                /*
                버튼 해제 후 안정화

                기계식 버튼은 접점이 튀면서
                여러 번 HIGH/LOW 변화가 발생할 수 있다.

                따라서 해제 후 20ms 기다린다.
                */

                vTaskDelay(pdMS_TO_TICKS(20));


                /*
                해제 안정화 이후
                버튼이 다시 HIGH라면
                아직 눌린 상태로 판단한다.

                완전히 LOW가 될 때까지 반복 대기한다.
                */

                while (HAL_GPIO_ReadPin(
                           GPIOC,
                           GPIO_PIN_13) == GPIO_PIN_SET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }


                /*
                대기 중 들어온 잔여 Semaphore 제거

                Binary Semaphore는
                최대 하나의 신호만 보관한다.

                xSemaphoreTake(keySemaphore, 0)

                대기 시간 = 0

                Semaphore = 1이면
                - 즉시 Take 성공
                - Semaphore = 0

                Semaphore = 0이면
                - 즉시 실패
                - Blocked 진입하지 않음

                반환값은 사용하지 않는다.

                목적은 디바운싱 중 발생한
                잔여 신호를 제거하는 것이다.
                */

                (void)xSemaphoreTake(
                    keySemaphore,
                    0);
            }
        }


        /*
        ⑰ 다음 버튼 입력 대기

        for(;;) 처음으로 돌아간다.

        xSemaphoreTake() 재호출

        Semaphore = 0이면

        KeyTask : RUNNING → BLOCKED

        다시 다음 인터럽트를 기다린다.

        Semaphore = 1이면

        Take가 즉시 성공하여
        다음 이벤트 처리를 시작한다.
        */
    }
}


/*
⑨~⑬ EXTI 인터럽트 처리

이 함수는 KeyTask가 직접 호출하지 않는다.

PC13에서 EXTI 인터럽트가 발생하고
HAL_GPIO_EXTI_IRQHandler()가 처리할 때
Callback으로 호출된다.

실제 IRQ 진입 함수는
stm32f4xx_it.c에 정의한다.
*/

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /*
    ⑨ B1 버튼 인터럽트 발생

    PC13 LOW → HIGH

    EXTI13 Rising Edge 감지

    EXTI Pending Flag 설정

    NVIC EXTI15_10_IRQn 인터럽트 요청


    ⑩ CPU 인터럽트 진입

    현재 실행하던 Thread를 중단한다.

    CPU 하드웨어가 자동으로 저장

    R0~R3
    R12
    LR
    PC
    xPSR

    인터럽트 이전에 PSP를 사용했다면
    기본 Context는 해당 PSP Stack에 저장된다.

    CPU는 Handler Mode로 진입한다.

    Handler에서는 MSP를 사용한다.


    ⑪ EXTI ISR 실행

    EXTI15_10_IRQHandler()

    HAL_GPIO_EXTI_IRQHandler()

    HAL_GPIO_EXTI_Callback()

    이 함수는 인터럽트 문맥에서 실행된다.
    */

    if (GPIO_Pin == GPIO_PIN_13)
    {
        /*
        xHigherPriorityTaskWoken 초기화

        pdFALSE로 초기화한다.

        ISR에서 FreeRTOS API를 호출한 결과
        더 높은 우선순위 Task가 깨어났다면
        pdTRUE로 변경될 수 있다.

        이 변수는 Task 실행권 자체를
        저장하는 변수가 아니다.
        */

        BaseType_t xHigherPriorityTaskWoken =
            pdFALSE;


        /*
        ⑫ ISR에서 Binary Semaphore Give

        xSemaphoreGiveFromISR()

        Semaphore를 기다리는 Task 존재?

        YES

        - KeyTask 대기 해제
        - KeyTask BLOCKED → READY
        - 현재 실행 중이던 Task와 우선순위 비교

        KeyTask Priority가 더 높다면

        xHigherPriorityTaskWoken = pdTRUE

        NO

        - 기다리는 Task가 없음
        - Semaphore가 Empty이면 1로 보관
        - 이미 1이면 추가 Give 실패 가능

        중요한 점

        GiveFromISR()는 KeyTask를
        직접 RUNNING으로 만드는 함수가 아니다.

        먼저 READY 상태로 변경한다.
        */

        xSemaphoreGiveFromISR(
            keySemaphore,
            &xHigherPriorityTaskWoken);


        /*
        ⑬ Context Switching 필요 여부 확인

        xHigherPriorityTaskWoken = pdTRUE ?

        YES

        portYIELD_FROM_ISR(pdTRUE)

        Cortex-M4 FreeRTOS 포트에서
        PendSV Pending 설정 요청

        SCB->ICSR의 PENDSVSET 비트 사용

        ISR 종료 이후 PendSV가 실행 가능하면
        Context Switching 수행


        NO

        이 호출에서는 PendSV 요청하지 않음

        ISR 종료 후 기존 실행 흐름으로
        복귀할 수 있다.

        KeyTask가 READY가 되었다면
        이후 Scheduler가 선택할 때 실행된다.


        PendSV가 실행되는 경우

        1. 현재 Task의 R4~R11 SW 저장

        2. 현재 Task PSP를 TCB에 저장

        3. vTaskSwitchContext()

        4. 다음 Ready Task 선택

        5. KeyTask가 선택되면
           KeyTask PSP 로드

        6. R4~R11 SW 복원

        7. Exception Return

        8. CPU가 기본 Context HW 복원

        9. KeyTask READY → RUNNING

        이때 KeyTask가 대기하던
        xSemaphoreTake()가 실행을 재개한다.

        참고

        EXTI 진입에서 이미 기본 레지스터를
        HW가 저장했다면 Tail-Chaining을 통해
        PendSV에서 동일한 기본 Context를
        다시 저장하지 않을 수 있다.

        FPU 사용 시에는 추가 FP Context
        저장·복원이 필요할 수 있다.
        */

        portYIELD_FROM_ISR(
            xHigherPriorityTaskWoken);
    }
}


/*
GPIO 초기화

LD2 PB7
B1 PC13

EXTI 우선순위만 설정한다.

IRQ 활성화는 KeyTask에서 수행한다.
*/

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /*
    GPIOB와 GPIOC 클럭 활성화
    */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();


    /*
    LD2 = PB7

    출력 초기 상태 LOW
    Push-Pull 출력
    */

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_7,
        GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


    /*
    B1 = PC13

    GPIO_MODE_IT_RISING

    PC13 LOW → HIGH 변화 시
    EXTI 인터럽트 요청을 발생시킨다.
    */

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);


    /*
    EXTI15_10_IRQn 우선순위 설정

    NVIC Priority = 5

    FreeRTOSConfig.h의
    configMAX_SYSCALL_INTERRUPT_PRIORITY와
    호환되는 우선순위여야 한다.

    여기서는 Enable하지 않는다.
    */

    HAL_NVIC_SetPriority(
        EXTI15_10_IRQn,
        5,
        0);
}


/*
USART3 초기화

PD8 = TX
PD9 = RX

115200 baud
8 Data Bits
No Parity
1 Stop Bit
*/

static void MX_USART3_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();


    /*
    PD8 / PD9 Alternate Function 7

    GPIO_MODE_AF_PP

    USART3 기능으로 핀을 연결한다.
    */

    GPIO_InitStruct.Pin =
        GPIO_PIN_8 | GPIO_PIN_9;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Alternate =
        GPIO_AF7_USART3;

    HAL_GPIO_Init(
        GPIOD,
        &GPIO_InitStruct);


    /*
    USART3 통신 설정
    */

    huart3.Instance = USART3;

    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;

    huart3.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart3.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}


/*
System Clock 설정

HSI 16MHz 사용
PLL 비활성화

SYSCLK = HSI
HCLK = SYSCLK
PCLK1 = HCLK
PCLK2 = HCLK
*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


    /*
    HSI Oscillator 활성화
    */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(
            &RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }


    /*
    시스템 클럭 소스 선택
    */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}


/*
Error Handler

초기화 실패 또는 Task 생성 실패 시 실행한다.

인터럽트를 비활성화하고
무한 루프에서 정지한다.
*/

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
