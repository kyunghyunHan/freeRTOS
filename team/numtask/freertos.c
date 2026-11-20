
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "usart.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * STM32F429ZI
 * FreeRTOS CMSIS-RTOS V1
 *
 * Task 함수: Num_Task() 한 개
 * Task 개수: 9개
 * Argument: 1 ~ 9
 * 출력 간격: 100 ~ 500ms
 * 공유 자원: USART3
 * 보호 방법: Mutex
 */

/* =========================================
 * 전역 변수
 * ========================================= */

osMutexId uartMutexHandle;

/* Task에 전달할 고유 번호 */
static uint32_t taskNumbers[9] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9
};

/* =========================================
 * Task 함수 선언
 * ========================================= */

void Num_Task(void const *argument);

/* =========================================
 * FreeRTOS 초기화
 * ========================================= */

void MX_FREERTOS_Init(void)
{
    /* 1. Mutex 생성 */
    osMutexDef(UartMutex);
    uartMutexHandle = osMutexCreate(osMutex(UartMutex));

    configASSERT(uartMutexHandle != NULL);

    /* 2. Random Seed 설정 */
    srand(1234);

    /* 3. Task 함수 정의 (한 개) */
    osThreadDef(
        NumTask,
        Num_Task,
        osPriorityNormal,
        0,
        1024
    );

    /* 4. 같은 함수로 Task 9개 생성 */
    for (int i = 0; i < 9; i++)
    {
        osThreadId taskHandle;

        taskHandle = osThreadCreate(
            osThread(NumTask),
            &taskNumbers[i]
        );

        configASSERT(taskHandle != NULL);
    }
}

/* =========================================
 * Task 실행 함수
 * ========================================= */

void Num_Task(void const *argument)
{
    uint32_t number = *(const uint32_t *)argument;

    uint32_t delayTime;

    char msg[64];
    int len;

    for (;;)
    {
        /* Mutex 획득 */
        if (osMutexWait(uartMutexHandle, osWaitForever) == osOK)
        {
            /*
             * rand()는 내부 상태를 공유할 수 있으므로
             * Mutex 내부에서 호출
             *
             * 랜덤 시간: 100 ~ 500ms
             */
            delayTime = 100U + (uint32_t)(rand() % 401);

            /* Task 번호 출력 문자열 */
            len = snprintf(
                msg,
                sizeof(msg),
                "Task %lu : %lu\r\n",
                (unsigned long)number,
                (unsigned long)number
            );

            /* UART 출력 */
            if (len > 0 && len < (int)sizeof(msg))
            {
                HAL_UART_Transmit(
                    &huart3,
                    (uint8_t *)msg,
                    (uint16_t)len,
                    100
                );
            }

            /* Mutex 반환 */
            osMutexRelease(uartMutexHandle);
        }
        else
        {
            delayTime = 100U;
        }

        /* 랜덤 시간만큼 Task Blocked */
        osDelay(delayTime);
    }
}
