StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,
    TaskFunction_t pxCode,
    void *pvParameters)
{
    /*
     * 새로 생성된 Task의 초기 Stack Frame을 구성하는 함수다.
     *
     * xTaskCreate() 과정에서 호출되며,
     * Task가 처음부터 실행될 수 있도록
     * CPU 레지스터의 초기 Context를 Stack 메모리에 배치한다.
     *
     * 아직 Task가 실제로 실행된 것은 아니다.
     * Cortex-M4F의 Exception Return 과정에서
     * 복원할 레지스터 값을 소프트웨어로 미리 생성한다.
     *
     * pxTopOfStack:
     * Task에 할당된 Stack 영역의 초기 상단 주소다.
     * Cortex-M4F의 Stack은 높은 주소에서 낮은 주소로 성장한다.
     *
     * pxCode:
     * Task 함수의 시작 주소를 저장한 함수 포인터다.
     * 예: TaskA 함수의 주소
     *
     * pvParameters:
     * Task 함수에 전달할 인자의 주소다.
     * 예: xTaskCreate()의 네 번째 인자
     *
     * 반환값:
     * 초기 Context 구성이 완료된 Stack Pointer다.
     * 이 주소는 TCB의 pxTopOfStack에 저장된다.
     */

    /*
     * Stack Pointer를 StackType_t 한 칸만큼 감소시킨다.
     *
     * Cortex-M4F에서 StackType_t는 일반적으로 uint32_t이므로
     * pxTopOfStack--는 주소를 4바이트 감소시킨다.
     *
     * 이 추가 Offset은 Cortex-M의 Exception 진입 및 복귀 시
     * Stack 사용 방식과 정렬 조건을 고려한 것이다.
     *
     * 예:
     * 실행 전 pxTopOfStack = 0x20002044
     * 실행 후 pxTopOfStack = 0x20002040
     */
    pxTopOfStack--;

    /*
     * 현재 Stack 위치에 초기 xPSR 값을 기록한다.
     *
     * xPSR:
     * Program Status Register
     *
     * APSR, IPSR, EPSR의 상태를 포함하는 레지스터다.
     *
     * portINITIAL_XPSR은 일반적으로 0x01000000이다.
     *
     * bit[24] = T(Thumb State)
     *
     * Cortex-M4F는 Thumb 명령어를 실행하므로
     * 최초 Task 실행 시 T비트가 설정되어 있어야 한다.
     *
     * 예:
     * [0x20002040] = 0x01000000
     */
    *pxTopOfStack = portINITIAL_XPSR;

    /*
     * Stack Pointer를 4바이트 감소시킨다.
     *
     * 다음 위치에는 최초 Task의 PC 값을 저장한다.
     */
    pxTopOfStack--;

    /*
     * Task 함수 시작 주소를 PC 슬롯에 저장한다.
     *
     * pxCode는 xTaskCreate()에 전달한 Task 함수 포인터다.
     *
     * (StackType_t) pxCode:
     * 함수 포인터를 Stack에 저장할 정수형 주소 값으로 변환한다.
     *
     * portSTART_ADDRESS_MASK:
     * Task 시작 주소에 적용하는 주소 마스크다.
     * 이 ARM_CM4F Port에서는 함수 포인터의 bit[0]을 제거한다.
     *
     * 예:
     * pxCode = 0x08001235
     * portSTART_ADDRESS_MASK = 0xFFFFFFFE
     *
     * 0x08001235 & 0xFFFFFFFE = 0x08001234
     *
     * [0x2000203C] = 0x08001234
     *
     * 나중에 Exception Return 시 CPU가 이 값을 PC로 복원한다.
     * 따라서 Task 함수의 첫 명령어부터 실행하게 된다.
     *
     * 주의:
     * 함수 포인터의 bit[0]은 Thumb 상태를 나타내는 데 사용된다.
     * 실제 명령어 주소가 홀수 바이트 위치라는 의미는 아니다.
     */
    *pxTopOfStack =
        ((StackType_t)pxCode) & portSTART_ADDRESS_MASK;

    /*
     * Stack Pointer를 다시 4바이트 감소시킨다.
     *
     * 다음 위치에는 최초 Task의 LR 값을 저장한다.
     */
    pxTopOfStack--;

    /*
     * Task의 초기 LR(Link Register) 값을 설정한다.
     *
     * portTASK_RETURN_ADDRESS는 일반적으로
     * prvTaskExitError 함수의 주소를 사용한다.
     *
     * FreeRTOS Task 함수는 일반적으로 return해서는 안 된다.
     *
     * 만약 Task 함수가 정상 종료처럼 return을 실행하면
     * LR에 저장된 주소로 복귀하려고 한다.
     *
     * 이때 prvTaskExitError가 실행되어
     * 잘못된 Task 반환을 감지할 수 있도록 한다.
     *
     * 이 LR은 나중에 vPortSVCHandler에서 복원하는
     * EXC_RETURN과는 완전히 다른 값이다.
     *
     * 예:
     * [0x20002038] = prvTaskExitError 함수 주소
     */
    *pxTopOfStack = (StackType_t)portTASK_RETURN_ADDRESS;

    /*
     * R12, R3, R2, R1에 해당하는 Stack 공간을 확보한다.
     *
     * Cortex-M4F에서 StackType_t는 4바이트다.
     *
     * pxTopOfStack -= 5;
     *
     * 5 × 4 = 20바이트 감소
     *
     * 주석에 나열된 R12, R3, R2, R1은 4개지만
     * R0 슬롯까지 포함하기 위해 5칸 이동한다.
     *
     * 이 레지스터들은 최초 Task 실행 전에
     * 특별한 초기값이 필요하지 않으므로 값을 기록하지 않는다.
     *
     * 레지스터를 0으로 초기화하는 것이 아니라
     * Stack 공간만 확보한다는 점이 중요하다.
     *
     * 예:
     * 이동 전 pxTopOfStack = 0x20002038
     * 이동 후 pxTopOfStack = 0x20002024
     *
     * 주소 배치:
     *
     * 0x20002034 : R12
     * 0x20002030 : R3
     * 0x2000202C : R2
     * 0x20002028 : R1
     * 0x20002024 : R0
     */
    pxTopOfStack -= 5;

    /*
     * Task 함수에 전달할 인자를 R0 슬롯에 저장한다.
     *
     * ARM AAPCS 호출 규약에서 첫 번째 함수 인자는
     * R0 레지스터를 통해 전달된다.
     *
     * FreeRTOS Task 함수 형식:
     *
     * void TaskA(void *pvParameters);
     *
     * 따라서 pvParameters를 R0 슬롯에 저장한다.
     *
     * 나중에 첫 Task의 Exception Return이 발생하면
     * CPU가 이 값을 R0로 복원한다.
     *
     * 결과적으로 TaskA(pvParameters)를 호출한 것과
     * 같은 인자 전달 상태가 만들어진다.
     *
     * 예:
     * pvParameters = 0x20003000
     *
     * [0x20002024] = 0x20003000
     */
    *pxTopOfStack = (StackType_t)pvParameters;

    /*
     * Stack Pointer를 4바이트 감소시킨다.
     *
     * 다음 위치에는 EXC_RETURN 값을 저장한다.
     */
    pxTopOfStack--;

    /*
     * 최초 Task의 EXC_RETURN 값을 저장한다.
     *
     * portINITIAL_EXC_RETURN은 일반적으로 0xFFFFFFFD다.
     *
     * EXC_RETURN은 일반적인 함수 복귀 주소가 아니다.
     * Cortex-M4F가 Exception Return 동작을 수행하도록
     * 지정하는 특수한 값이다.
     *
     * 0xFFFFFFFD의 주요 의미:
     *
     * bit[4] = 1 : Basic Stack Frame 사용
     * bit[3] = 1 : Thread Mode로 복귀
     * bit[2] = 1 : PSP 사용
     *
     * 이 값은 나중에 vPortSVCHandler()에서
     * LDMIA 명령어를 통해 R14(LR)로 복원된다.
     *
     * 이후 BX R14를 실행하면
     * Cortex-M4F 하드웨어가 Exception Return을 수행한다.
     *
     * 예:
     * [0x20002020] = 0xFFFFFFFD
     */
    *pxTopOfStack = portINITIAL_EXC_RETURN;

    /*
     * R11부터 R4까지 8개 레지스터의 Stack 공간을 확보한다.
     *
     * pxTopOfStack -= 8;
     *
     * 8 × 4 = 32바이트 감소
     *
     * Cortex-M4F의 Exception Entry 하드웨어는
     * R0~R3, R12, LR, PC, xPSR을 자동 저장하지만
     * R4~R11은 자동 저장하지 않는다.
     *
     * 따라서 FreeRTOS Port는 Context Switching 시
     * R4~R11을 소프트웨어로 저장하고 복원한다.
     *
     * 최초 Task는 아직 실행된 적이 없으므로
     * R4~R11의 초기값은 특별한 의미가 없다.
     *
     * 이 함수에서는 값을 기록하지 않고
     * 해당 레지스터를 복원할 공간만 확보한다.
     *
     * 예:
     * 이동 전 pxTopOfStack = 0x20002020
     * 이동 후 pxTopOfStack = 0x20002000
     *
     * 주소 배치:
     *
     * 0x20002000 : R4
     * 0x20002004 : R5
     * 0x20002008 : R6
     * 0x2000200C : R7
     * 0x20002010 : R8
     * 0x20002014 : R9
     * 0x20002018 : R10
     * 0x2000201C : R11
     */
    pxTopOfStack -= 8;

    /*
     * 초기 CPU Context의 시작 주소를 반환한다.
     *
     * 예:
     * 반환값 = 0x20002000
     *
     * xTaskCreate() 내부에서는 이 반환값을
     * Task TCB의 pxTopOfStack에 저장한다.
     *
     * 이후 최초 Task 실행 과정:
     *
     * vPortSVCHandler()
     *   → pxCurrentTCB 읽기
     *   → TCB의 pxTopOfStack 읽기
     *   → LDMIA로 R4~R11 및 EXC_RETURN 복원
     *   → PSP 설정
     *   → BX LR
     *   → CPU가 R0~R3, R12, LR, PC, xPSR 복원
     *   → Task 함수 최초 실행
     */
    return pxTopOfStack;
}

void vTaskStartScheduler(void)
{
    // Idle Task 및 Timer Task 생성 결과를 저장하는 변수
    // pdPASS: 성공, pdFAIL: 실패
    BaseType_t xReturn;

    // Idle Task를 가장 낮은 우선순위(0)로 생성한다.
    // Idle Task는 실행 가능한 다른 Task가 없을 때 실행되며,
    // 삭제된 Task의 메모리 정리 등 커널 내부 작업을 수행한다.

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    {
        // 정적 메모리 할당이 활성화된 경우
        // 애플리케이션이 제공한 RAM을 사용하여 Idle Task를 생성한다.

        // Idle Task의 TCB 메모리 주소를 저장할 포인터
        StaticTask_t *pxIdleTaskTCBBuffer = NULL;

        // Idle Task의 Stack 메모리 주소를 저장할 포인터
        StackType_t *pxIdleTaskStackBuffer = NULL;

        // Idle Task의 Stack 크기 (바이트가 아닌 StackType_t 단위)
        uint32_t ulIdleTaskStackSize;

        // 애플리케이션이 준비한 TCB 주소, Stack 주소, Stack 크기를 가져온다.
        // 인수에 &를 사용하는 이유는 함수 내부에서 포인터 변수와 크기 값을
        // 직접 설정하도록 주소를 전달하기 때문이다.
        vApplicationGetIdleTaskMemory(
            &pxIdleTaskTCBBuffer,
            &pxIdleTaskStackBuffer,
            &ulIdleTaskStackSize);

        // 사용자가 제공한 TCB와 Stack 메모리로 Idle Task를 생성한다.
        // prvIdleTask: Idle Task가 실행할 내부 함수
        // configIDLE_TASK_NAME: Task 이름
        // ulIdleTaskStackSize: Stack 크기
        // NULL: Task에 전달할 인자가 없음
        // portPRIVILEGE_BIT: 권한 설정 비트 (일반 Cortex-M4F 포트에서는 0)
        // pxIdleTaskStackBuffer: Stack 메모리 주소
        // pxIdleTaskTCBBuffer: TCB 메모리 주소
        xIdleTaskHandle = xTaskCreateStatic(
            prvIdleTask,
            configIDLE_TASK_NAME,
            ulIdleTaskStackSize,
            (void *)NULL,
            portPRIVILEGE_BIT,
            pxIdleTaskStackBuffer,
            pxIdleTaskTCBBuffer);

        // Task Handle이 NULL이 아니면 Idle Task 생성 성공
        if (xIdleTaskHandle != NULL)
        {
            xReturn = pdPASS;
        }
        else
        {
            xReturn = pdFAIL;
        }
    }

#else
    {
        // 정적 할당이 비활성화된 경우 동적 메모리 할당을 사용한다.
        // FreeRTOS Heap에서 Idle Task의 TCB와 Stack 공간을 확보한다.
        // configMINIMAL_STACK_SIZE는 StackType_t 단위의 최소 Stack 크기다.
        // &xIdleTaskHandle을 전달하여 생성된 Task Handle을 저장한다.
        xReturn = xTaskCreate(
            prvIdleTask,
            configIDLE_TASK_NAME,
            configMINIMAL_STACK_SIZE,
            (void *)NULL,
            portPRIVILEGE_BIT,
            &xIdleTaskHandle);
    }
#endif /* configSUPPORT_STATIC_ALLOCATION */

// FreeRTOS Software Timer 기능이 활성화되어 있으면
// Timer Service Task(Daemon Task)를 생성한다.
#if (configUSE_TIMERS == 1)
    {
        // Idle Task 생성에 성공했을 때만 Timer Task 생성 시도
        if (xReturn == pdPASS)
        {
            // Software Timer 만료 처리와 Timer Command Queue 처리를
            // 담당할 Timer Service Task를 생성한다.
            xReturn = xTimerCreateTimerTask();
        }
        else
        {
            // 코드 커버리지 테스트용 매크로
            // 일반적인 실행에서는 별도의 동작이 없을 수 있다.
            mtCOVERAGE_TEST_MARKER();
        }
    }
#endif /* configUSE_TIMERS */

    // 필수 시스템 Task 생성에 성공한 경우에만 Scheduler를 시작한다.
    if (xReturn == pdPASS)
    {
// 사용자가 FREERTOS_TASKS_C_ADDITIONS_INIT 매크로를 정의한 경우
// 추가적인 커널 초기화 코드를 실행한다.
#ifdef FREERTOS_TASKS_C_ADDITIONS_INIT
        {
            freertos_tasks_c_additions_init();
        }
#endif

        // Scheduler가 시작되는 도중 SysTick Interrupt가 발생하면
        // 아직 CPU Context가 준비되지 않은 상태에서
        // 커널 Tick 처리가 실행될 수 있으므로 Interrupt를 비활성화한다.
        //
        // Cortex-M4F FreeRTOS Port에서는 일반적으로 BASEPRI를 이용해
        // 특정 우선순위 범위의 Interrupt를 마스킹한다.
        // 모든 Interrupt를 무조건 비활성화하는 것은 아니다.
        portDISABLE_INTERRUPTS();

#if (configUSE_NEWLIB_REENTRANT == 1)
        {
            // Newlib C 라이브러리의 재진입 상태를
            // 최초 실행할 Task의 전용 상태 구조체와 연결한다.
            //
            // pxCurrentTCB: 현재 실행 대상으로 선택된 Task의 TCB 포인터
            // xNewLib_reent: Task별 Newlib 재진입 상태 저장 공간
            // _impure_ptr: Newlib가 현재 재진입 상태를 참조하는 포인터
            //
            // 이를 통해 errno 등 Newlib의 일부 상태를
            // Task별로 독립적으로 관리할 수 있다.
            _impure_ptr = &(pxCurrentTCB->xNewLib_reent);
        }
#endif /* configUSE_NEWLIB_REENTRANT */

        // 다음에 Blocked 상태에서 해제될 Task의 Tick 시간을 초기화한다.
        // portMAX_DELAY는 TickType_t가 표현할 수 있는 최대값이다.
        // 이후 Tick 처리 과정에서 실제 해제 예정 시간으로 갱신된다.
        xNextTaskUnblockTime = portMAX_DELAY;

        // FreeRTOS Scheduler가 실행 중임을 나타내는 커널 상태 변수
        // 이 값이 pdTRUE가 되더라도 아직 첫 Task가 실행된 것은 아니다.
        xSchedulerRunning = pdTRUE;

        // 커널 Tick Count를 설정된 초기값으로 설정한다.
        // 이후 SysTick Handler에서 Tick이 증가한다.
        xTickCount = (TickType_t)configINITIAL_TICK_COUNT;

        // Task별 CPU 실행 시간 통계 기능을 사용할 때
        // 측정에 필요한 Timer/Counter를 초기화한다.
        // 기능을 사용하지 않는 설정에서는 빈 매크로일 수 있다.
        portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

        // Trace 기능이 활성화된 경우 최초 실행 대상으로 선택된
        // Task의 실행 시작 이벤트를 기록한다.
        // 실제 CPU가 Task 함수를 실행하기 전의 Hook 호출이다.
        traceTASK_SWITCHED_IN();

        // CPU 아키텍처별 Scheduler 시작 함수
        //
        // Cortex-M4F GCC Port에서는 port.c에 구현되어 있다.
        //
        // 주요 내부 실행 과정:
        //
        // xPortStartScheduler()
        //   → Interrupt Priority 설정 및 검증
        //   → PendSV / SysTick 우선순위 설정
        //   → vPortSetupTimerInterrupt()
        //   → FPU 관련 설정
        //   → prvPortStartFirstTask()
        //   → SVC #0
        //   → vPortSVCHandler()
        //   → 최초 Task의 Context 복원
        //   → Exception Return
        //   → 첫 번째 Task 실행
        //
        // Scheduler가 정상 실행 중이면
        // 일반적으로 이 함수는 반환되지 않는다.
        if (xPortStartScheduler() != pdFALSE)
        {
            // 정상적으로 Scheduler가 동작 중이라면
            // 이 위치까지 실행 흐름이 돌아오지 않아야 한다.
        }
        else
        {
            // Port에서 Scheduler 종료를 지원하고
            // xTaskEndScheduler() 등으로 종료한 경우 반환될 수 있다.
            // 일반적인 Cortex-M4F FreeRTOS Port는
            // Scheduler 종료를 지원하지 않는다.
        }
    }
    else
    {
        // Idle Task 또는 Timer Service Task의 생성에 실패한 경우
        // Scheduler를 시작하지 않는다.
        //
        // 동적 할당 방식에서는 FreeRTOS Heap 부족 등이 원인일 수 있다.
        // ASSERT 조건이 거짓이고 configASSERT가 활성화되어 있다면
        // 오류를 감지하여 디버깅할 수 있다.
        configASSERT(xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY);
    }

    // xIdleTaskHandle이 다른 코드에서 사용되지 않는 설정일 때
    // 컴파일러의 미사용 변수 경고를 방지하기 위한 명시적 참조
    // 실제 Task 실행 동작에는 영향을 주지 않는다.
    (void)xIdleTaskHandle;
}

BaseType_t xPortStartScheduler(void)
{
    /*
     * configMAX_SYSCALL_INTERRUPT_PRIORITY는 0이면 안 된다.
     *
     * 이 값은 FreeRTOS의 FromISR API를 안전하게 호출할 수 있는
     * 인터럽트 우선순위 범위를 제한하는 데 사용된다.
     *
     * Cortex-M에서는 우선순위 숫자가 작을수록 긴급도가 높다.
     * 우선순위 0은 BASEPRI로 마스킹할 수 없으므로
     * configMAX_SYSCALL_INTERRUPT_PRIORITY를 0으로 설정하면 안 된다.
     */
    configASSERT(configMAX_SYSCALL_INTERRUPT_PRIORITY);

    /*
     * Cortex-M7 r0p0 및 r0p1 코어는 이 Port에서 지원하지 않는다.
     *
     * 해당 코어는 별도의 ARM_CM7/r0p1 Port를 사용해야 한다.
     * STM32F429ZI는 Cortex-M4F이므로 이 조건에 해당하지 않는다.
     *
     * portCPUID는 SCB의 CPUID 레지스터 값을 읽는 매크로다.
     */
    configASSERT(portCPUID != portCORTEX_M7_r0p1_ID);
    configASSERT(portCPUID != portCORTEX_M7_r0p0_ID);

#if (configASSERT_DEFINED == 1)
    {
        // 검사 과정에서 잠시 변경할 NVIC 우선순위 레지스터의 원래 값
        volatile uint32_t ulOriginalPriority;

        /*
         * 첫 번째 외부 인터럽트의 Priority Register 주소를 가리킨다.
         *
         * volatile:
         * 해당 주소가 하드웨어 레지스터이므로 메모리 접근을 유지한다.
         *
         * uint8_t:
         * NVIC의 인터럽트 우선순위 레지스터는
         * 인터럽트 하나당 8비트 필드를 사용한다.
         *
         * *const:
         * pucFirstUserPriorityRegister 포인터 자체는 재지정할 수 없다.
         * 하지만 포인터가 가리키는 레지스터 값은 수정할 수 있다.
         *
         * 예를 들어:
         * pucFirstUserPriorityRegister = 다른주소;  // 불가능
         * *pucFirstUserPriorityRegister = 0xFF;     // 가능
         */
        volatile uint8_t *const pucFirstUserPriorityRegister =
            (volatile uint8_t *const)(portNVIC_IP_REGISTERS_OFFSET_16 +
                                      portFIRST_USER_INTERRUPT_NUMBER);

        // NVIC에서 실제로 구현된 Priority 비트를 확인하기 위한 변수
        volatile uint8_t ucMaxPriorityValue;

        /*
         * 현재 인터럽트 Priority Register 값을 백업한다.
         *
         * 이후 실제 구현된 Priority 비트 수를 검사하기 위해
         * 이 레지스터에 0xFF를 기록하므로 원래 값을 보관해야 한다.
         */
        ulOriginalPriority = *pucFirstUserPriorityRegister;

        /*
         * Priority Register에 모든 비트를 1로 기록한다.
         *
         * Cortex-M의 Priority Register는 8비트 공간을 갖지만
         * MCU에 따라 실제 구현된 비트 수가 다르다.
         *
         * STM32F429ZI는 일반적으로 4비트가 구현되어 있으므로
         * 0xFF를 기록해도 구현되지 않은 하위 비트는 유지되지 않는다.
         */
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

        /*
         * 방금 기록한 값을 다시 읽는다.
         *
         * STM32F429ZI처럼 Priority 비트가 4개라면
         * 일반적으로 읽은 값은 0xF0이 된다.
         *
         * 즉, 상위 4비트만 실제 하드웨어에서 구현되었음을 확인한다.
         */
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;

        /*
         * configMAX_SYSCALL_INTERRUPT_PRIORITY에
         * 실제 구현된 Priority 비트 마스크를 적용한다.
         *
         * 예:
         * configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50
         * ucMaxPriorityValue                 = 0xF0
         *
         * 0x50 & 0xF0 = 0x50
         *
         * ucMaxSysCallPriority는 이후 인터럽트 우선순위
         * 유효성 검사에 사용된다.
         */
        ucMaxSysCallPriority =
            configMAX_SYSCALL_INTERRUPT_PRIORITY &
            ucMaxPriorityValue;

        /*
         * NVIC Priority Grouping이 사용할 수 있는
         * 최대 허용값을 계산하기 위한 초기값을 설정한다.
         *
         * FreeRTOS는 FromISR API의 우선순위 검증을 위해
         * 구현된 Priority 비트 수와 Priority Grouping을 확인한다.
         */
        ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS;

        /*
         * 실제 구현된 Priority 비트 수를 계산한다.
         *
         * 예: ucMaxPriorityValue = 0xF0
         *
         * 11110000 → 11100000 → 11000000
         *          → 10000000 → 00000000
         *
         * 상위 비트가 1인 동안 왼쪽으로 Shift하고
         * ulMaxPRIGROUPValue를 감소시킨다.
         *
         * 0xF0이면 반복 횟수는 4회다.
         */
        while (
            (ucMaxPriorityValue & portTOP_BIT_OF_BYTE) == portTOP_BIT_OF_BYTE)
        {
            ulMaxPRIGROUPValue--;

            // 1비트 왼쪽 Shift
            ucMaxPriorityValue <<= (uint8_t)0x01;
        }

#ifdef __NVIC_PRIO_BITS
        {
            /*
             * CMSIS가 정의한 NVIC Priority 비트 수와
             * 하드웨어에서 직접 확인한 비트 수를 비교한다.
             *
             * STM32F429ZI의 경우 __NVIC_PRIO_BITS는 4다.
             *
             * 서로 다르면 MCU 헤더 파일 설정이나
             * 실제 CPU 설정이 일치하지 않는다는 의미다.
             */
            configASSERT(
                (portMAX_PRIGROUP_BITS - ulMaxPRIGROUPValue) == __NVIC_PRIO_BITS);
        }
#endif

#ifdef configPRIO_BITS
        {
            /*
             * FreeRTOSConfig.h에 정의된 configPRIO_BITS와
             * 실제 NVIC Priority 비트 수를 비교한다.
             *
             * STM32F429ZI에서는 일반적으로 4로 설정한다.
             */
            configASSERT(
                (portMAX_PRIGROUP_BITS - ulMaxPRIGROUPValue) == configPRIO_BITS);
        }
#endif

        /*
         * 계산한 Priority Grouping 값을
         * SCB AIRCR 레지스터의 PRIGROUP 필드 위치로 이동한다.
         *
         * PRIGROUP 필드는 AIRCR의 bit[10:8]에 위치한다.
         */
        ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;

        /*
         * AIRCR의 PRIGROUP 필드에 해당하는 비트만 남긴다.
         *
         * 이 값은 이후 인터럽트 우선순위 설정이
         * FreeRTOS 요구 조건을 만족하는지 검사할 때 사용한다.
         *
         * 여기서 AIRCR 레지스터에 직접 기록하는 것은 아니다.
         */
        ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;

        /*
         * 앞에서 0xFF로 변경했던 인터럽트 Priority Register를
         * 백업한 원래 값으로 복원한다.
         *
         * 실제 우선순위 비트 수를 확인하기 위한 임시 접근이므로
         * 기존 인터럽트 설정은 유지해야 한다.
         */
        *pucFirstUserPriorityRegister = ulOriginalPriority;
    }
#endif /* configASSERT_DEFINED */

    /*
     * PendSV와 SysTick의 우선순위를 가장 낮게 설정한다.
     *
     * PendSV:
     * Task Context Switching을 수행하는 Exception이다.
     *
     * SysTick:
     * FreeRTOS Tick 처리를 위한 주기적인 Exception이다.
     *
     * portNVIC_SYSPRI2_REG는 SCB의 SHPR3 레지스터를 가리킨다.
     *
     * PendSV Priority 필드: bit[23:16]
     * SysTick Priority 필드: bit[31:24]
     *
     * 두 Exception의 우선순위를 가장 낮게 설정하면
     * 더 긴급한 Interrupt 처리를 우선 수행할 수 있다.
     */
    portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;

    portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

    /*
     * FreeRTOS Tick을 발생시키는 하드웨어 Timer를 설정한다.
     *
     * 기본 Cortex-M4F Port에서는 SysTick을 사용한다.
     *
     * 이 함수 내부에서 일반적으로 다음을 설정한다.
     *
     * - SysTick LOAD: Tick 주기
     * - SysTick VAL: 현재 Counter 초기화
     * - SysTick CTRL: Counter 및 Interrupt 활성화
     *
     * 예: CPU Clock = 16MHz, Tick Rate = 1000Hz
     * LOAD = 16000 - 1 = 15999
     *
     * 실제 LOAD 값은 해당 프로젝트의 클록 설정에 따라 달라진다.
     *
     * 이 시점에는 Scheduler 시작을 위한 Interrupt 마스킹이
     * 적용되어 있다.
     */
    vPortSetupTimerInterrupt();

    /*
     * Critical Section 중첩 횟수를 0으로 초기화한다.
     *
     * uxCriticalNesting은 portENTER_CRITICAL()과
     * portEXIT_CRITICAL()의 중첩 상태를 관리한다.
     *
     * 최초 Task가 실행되기 전에
     * Critical Section 중첩 상태를 초기화한다.
     */
    uxCriticalNesting = 0;

    /*
     * Cortex-M4F의 FPU를 활성화한다.
     *
     * STM32F429ZI는 부동소수점 연산을 지원하는
     * Cortex-M4F 코어를 사용한다.
     *
     * vPortEnableVFP()는 CPACR의 CP10/CP11 접근 권한을
     * 설정하여 FPU 명령어를 사용할 수 있게 한다.
     */
    vPortEnableVFP();

    /*
     * FPU Lazy Stacking 기능을 활성화한다.
     *
     * FPCCR:
     * Floating-Point Context Control Register
     *
     * ASPEN:
     * Automatic State Preservation Enable
     *
     * LSPEN:
     * Lazy State Preservation Enable
     *
     * Floating Point Context를 예외마다 무조건 즉시 저장하지 않고,
     * 실제 필요 시점까지 일부 저장 작업을 지연시킬 수 있다.
     *
     * 이를 통해 불필요한 Floating Point Register 저장 비용을
     * 줄일 수 있다.
     */
    *(portFPCCR) |= portASPEN_AND_LSPEN_BITS;

    /*
     * 최초 Task 실행을 시작하기 위한 내부 함수
     *
     * prvPortStartFirstTask()는 ARM Assembly로 구현되어 있다.
     *
     * 주요 작업:
     *
     * 1. VTOR 레지스터에서 Vector Table 시작 주소 읽기
     * 2. Vector Table 첫 번째 Word에서 Initial MSP 읽기
     * 3. MSP 설정
     * 4. CONTROL 레지스터 초기화
     * 5. PRIMASK / FAULTMASK 마스킹 해제
     * 6. SVC #0 실행
     *
     * SVC Exception이 발생하면 vPortSVCHandler()가 실행된다.
     *
     * vPortSVCHandler()는 최초 Task의
     * Stack Context를 복원하고 PSP를 설정한다.
     *
     * 이후 EXC_RETURN으로 첫 번째 Task가 실행된다.
     *
     * 정상적으로 Task 실행이 시작되면
     * 아래 코드로 반환하지 않는다.
     */
    prvPortStartFirstTask();

    /*
     * 정상적인 실행에서는 여기까지 도달하지 않는다.
     *
     * vTaskSwitchContext() 호출을 남겨 두는 이유는
     * Link Time Optimization(LTO) 사용 시
     * 해당 심볼이 제거되지 않도록 하기 위해서다.
     *
     * 실제 Task 전환은 일반적으로 PendSV Handler에서
     * vTaskSwitchContext()를 호출하여 수행한다.
     */
    vTaskSwitchContext();

    /*
     * Task가 정상적이지 않게 반환하는 상황을 처리하는 함수
     *
     * FreeRTOS Task 함수는 일반적으로 return하면 안 된다.
     * 무한 루프로 동작하거나 필요 시 vTaskDelete()를 호출해야 한다.
     *
     * 이 호출은 정상 실행 경로가 아니라
     * 컴파일러 및 링커 최적화 관련 처리를 위한 코드다.
     */
    prvTaskExitError();

    // 정상적으로 Scheduler가 시작되었다면 도달하지 않는 코드
    return 0;
}

__attribute__((weak)) void vPortSetupTimerInterrupt(void)
{
    /*
     * FreeRTOS Scheduler가 사용할 SysTick Timer를 초기화한다.
     *
     * __attribute__(( weak )):
     * GCC의 Weak Symbol 속성이다.
     *
     * 다른 소스 파일에서 동일한 이름의 Strong Symbol 함수를
     * 정의하면 이 기본 구현 대신 해당 함수를 사용할 수 있다.
     *
     * 이를 통해 애플리케이션에서 SysTick 대신 다른 Timer로
     * 커널 Tick을 발생시키도록 구현을 교체할 수 있다.
     *
     * 이 함수는 xPortStartScheduler()에서 호출된다.
     *
     * SysTick은 Cortex-M4F 코어에 내장된 24비트
     * Down Counter이며, 설정한 주기로 SysTick Exception을
     * 발생시켜 FreeRTOS의 Tick 처리를 수행하게 한다.
     */

#if (configUSE_TICKLESS_IDLE == 1)
    {
        /*
         * Tickless Idle 기능이 활성화된 경우에만 실행한다.
         *
         * Tickless Idle:
         * CPU가 장시간 Idle 상태로 들어갈 수 있을 때
         * 주기적인 SysTick Interrupt를 일시적으로 억제하여
         * 불필요한 CPU Wake-up과 전력 소비를 줄이는 기능이다.
         *
         * 다음 세 변수는 Tickless Idle 중 SysTick을 정지하거나
         * 재설정할 때 필요한 시간 계산에 사용된다.
         */

        /*
         * FreeRTOS Tick 1개에 필요한 SysTick Clock 수를 계산한다.
         *
         * configSYSTICK_CLOCK_HZ:
         * SysTick에 입력되는 Clock 주파수(Hz)
         *
         * configTICK_RATE_HZ:
         * 1초 동안 발생시키려는 FreeRTOS Tick 수
         *
         * 예:
         * configSYSTICK_CLOCK_HZ = 16,000,000Hz
         * configTICK_RATE_HZ = 1000Hz
         *
         * 16,000,000 / 1000 = 16,000
         *
         * 따라서 Tick 하나의 시간은 1ms이고
         * 그동안 16,000개의 SysTick Clock이 필요하다.
         */
        ulTimerCountsForOneTick =
            (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ);

        /*
         * SysTick의 24비트 Reload 범위에서
         * 한 번에 억제할 수 있는 최대 Tick 수를 계산한다.
         *
         * SysTick LOAD 레지스터는 24비트이므로
         * 설정할 수 있는 최대 Reload 값은 0xFFFFFF다.
         *
         * portMAX_24_BIT_NUMBER = 0xFFFFFF
         *
         * 예:
         * 0xFFFFFF = 16,777,215
         *
         * Tick당 Clock 수 = 16,000
         *
         * 16,777,215 / 16,000 = 1048
         *
         * 이 설정에서는 SysTick 카운터의 표현 범위 때문에
         * 한 번의 Tickless 구간에서 억제할 수 있는 Tick 수가
         * 최대 약 1048개로 제한된다.
         *
         * 실제 억제 시간은 다른 Task의 Wake-up 시점이나
         * Interrupt 발생 등에 따라 더 짧아질 수 있다.
         */
        xMaximumPossibleSuppressedTicks =
            portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;

        /*
         * Tickless Idle에서 SysTick을 정지하고 다시 설정하는
         * 과정의 지연을 보정하기 위한 Clock 수를 계산한다.
         *
         * portMISSED_COUNTS_FACTOR:
         * SysTick 정지 및 재시작에 따른 오차를 보정하기 위해
         * Port에서 정의한 경험적 보정 상수다.
         *
         * configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ:
         * CPU Clock과 SysTick Clock의 주파수 비율이다.
         *
         * 예:
         * CPU Clock = 16MHz
         * SysTick Clock = 16MHz
         *
         * 주파수 비율 = 1
         *
         * 따라서 보정값은 portMISSED_COUNTS_FACTOR와 같다.
         *
         * 이 값은 Tickless Idle의 경과 시간 계산에 사용되며,
         * 모든 하드웨어에서 정확한 지연 시간을 측정한 값은 아니다.
         */
        ulStoppedTimerCompensation =
            portMISSED_COUNTS_FACTOR /
            (configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ);
    }
#endif /* configUSE_TICKLESS_IDLE */

    /*
     * SysTick Control and Status Register를 0으로 설정한다.
     *
     * portNVIC_SYSTICK_CTRL_REG:
     * SysTick CTRL 레지스터에 접근하는 매크로다.
     *
     * Cortex-M4F 주소: 0xE000E010
     *
     * 주요 비트:
     *
     * bit[0] ENABLE:
     * 0 = SysTick Counter 비활성화
     * 1 = SysTick Counter 활성화
     *
     * bit[1] TICKINT:
     * 0 = Counter가 0에 도달해도 SysTick Exception 요청 안 함
     * 1 = Counter가 0에 도달하면 SysTick Exception 요청
     *
     * bit[2] CLKSOURCE:
     * 0 = 외부 Reference Clock 사용
     * 1 = Processor Clock 사용
     *
     * 이 명령은 SysTick을 일단 정지시키고
     * Exception 요청과 Clock Source 설정을 초기화한다.
     *
     * 주의:
     * CTRL 레지스터를 0으로 기록한다고
     * 현재 Counter 값까지 직접 0으로 만드는 것은 아니다.
     */
    portNVIC_SYSTICK_CTRL_REG = 0UL;

    /*
     * SysTick Current Value Register에 0을 기록한다.
     *
     * portNVIC_SYSTICK_CURRENT_VALUE_REG:
     * SysTick VAL 레지스터에 접근하는 매크로다.
     *
     * Cortex-M4F 주소: 0xE000E018
     *
     * VAL 레지스터에 값을 쓰면 현재 Counter 값이 0으로 초기화된다.
     *
     * 또한 CTRL 레지스터의 COUNTFLAG도 초기화된다.
     *
     * 따라서 이전에 남아 있던 Counter 상태를 제거하고
     * 새로운 Reload 값으로 Tick을 시작할 준비를 한다.
     */
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

    /*
     * SysTick Reload Value Register를 설정한다.
     *
     * portNVIC_SYSTICK_LOAD_REG:
     * SysTick LOAD 레지스터에 접근하는 매크로다.
     *
     * Cortex-M4F 주소: 0xE000E014
     *
     * LOAD에는 Tick 하나에 필요한 Clock 수에서
     * 1을 뺀 값을 기록한다.
     *
     * 공식:
     *
     * LOAD = (SysTick Clock / Tick Rate) - 1
     *
     * 예:
     * SysTick Clock = 16MHz
     * FreeRTOS Tick Rate = 1000Hz
     *
     * LOAD = (16,000,000 / 1000) - 1
     *      = 16,000 - 1
     *      = 15,999
     *
     * Reload 값이 15,999이면
     * 정상적인 주기 동작에서 16,000 Clock마다
     * SysTick Counter가 0에 도달한다.
     *
     * 따라서 1ms마다 SysTick Exception을 요청한다.
     *
     * 주의:
     * 실제 configSYSTICK_CLOCK_HZ 값은
     * 프로젝트의 System Clock 설정에 따라 달라진다.
     *
     * LOAD 값은 24비트 범위(0x000000~0xFFFFFF)에
     * 들어가야 한다.
     */
    portNVIC_SYSTICK_LOAD_REG =
        (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ) - 1UL;

    /*
     * SysTick Control Register에 필요한 비트를 설정한다.
     *
     * portNVIC_SYSTICK_CLK_BIT:
     * CLKSOURCE bit[2] = 1
     * Processor Clock을 SysTick Clock Source로 선택한다.
     *
     * portNVIC_SYSTICK_INT_BIT:
     * TICKINT bit[1] = 1
     * Counter가 0에 도달할 때 SysTick Exception을 요청한다.
     *
     * portNVIC_SYSTICK_ENABLE_BIT:
     * ENABLE bit[0] = 1
     * SysTick Counter를 활성화한다.
     *
     * 비트 연산:
     *
     * 0x04 | 0x02 | 0x01 = 0x07
     *
     * 따라서 CTRL 레지스터에는 일반적으로 0x07이 기록된다.
     *
     * bit[2] = 1 : Processor Clock 사용
     * bit[1] = 1 : SysTick Exception 활성화
     * bit[0] = 1 : SysTick Counter 활성화
     *
     * 이 시점부터 SysTick Counter가 동작한다.
     *
     * 다만 Scheduler 시작 과정에서 Interrupt 마스킹이
     * 적용되어 있을 수 있으므로, Counter가 활성화된 시점과
     * SysTick Handler가 실제 실행되는 시점은 다를 수 있다.
     *
     * 이후 SysTick Exception이 처리되면
     * xPortSysTickHandler()를 통해
     * FreeRTOS Tick Count 및 Task 상태가 갱신된다.
     */
    portNVIC_SYSTICK_CTRL_REG =
        (portNVIC_SYSTICK_CLK_BIT |
         portNVIC_SYSTICK_INT_BIT |
         portNVIC_SYSTICK_ENABLE_BIT);
}

static void prvPortStartFirstTask(void)
{
    /*
     * FreeRTOS의 첫 번째 Task 실행을 시작하는 내부 함수다.
     *
     * static:
     * 이 함수는 현재 소스 파일(port.c) 내부에서만 호출할 수 있다.
     *
     * 이 함수는 Task를 직접 호출하지 않는다.
     * MSP와 CONTROL 등 CPU 상태를 초기화한 후
     * SVC Exception을 발생시켜 vPortSVCHandler()로 진입한다.
     *
     * Scheduler 시작 전에 FPU를 사용했을 수 있으므로
     * CONTROL.FPCA를 0으로 초기화하여
     * 불필요한 Floating Point Context 저장을 방지한다.
     */

    __asm volatile(
        /*
         * __asm:
         * C 코드 안에 ARM 어셈블리 명령어를 작성한다.
         *
         * volatile:
         * 컴파일러가 이 어셈블리 블록을 불필요하다고 판단해
         * 제거하거나 일반적인 최적화 대상으로 취급하지 않도록 한다.
         *
         * 각 문자열의 \n은 다음 어셈블리 명령어와
         * 구분하기 위한 줄바꿈 문자다.
         */

        " ldr r0, =0xE000ED08 \n"
        /*
         * R0 = 0xE000ED08
         *
         * LDR:
         * Load Register
         *
         * =0xE000ED08:
         * 어셈블러가 상수를 레지스터에 로드하도록 만드는 의사 명령어
         *
         * 0xE000ED08:
         * SCB_VTOR(Vector Table Offset Register) 주소
         *
         * VTOR에는 현재 Vector Table의 시작 주소가 저장되어 있다.
         * 아직 VTOR의 값을 읽은 것이 아니라
         * VTOR 레지스터의 주소를 R0에 저장한 상태다.
         */

        " ldr r0, [r0] \n"
        /*
         * R0 = *(uint32_t *)R0
         *
         * [r0]:
         * R0에 저장된 주소가 가리키는 메모리를 읽는다.
         *
         * 예:
         * R0 = 0xE000ED08
         * [0xE000ED08] = 0x08000000
         *
         * 실행 후 R0 = 0x08000000
         *
         * 즉, VTOR에서 Vector Table 시작 주소를 읽는다.
         */

        " ldr r0, [r0] \n"
        /*
         * 다시 R0가 가리키는 메모리를 읽는다.
         *
         * 예:
         * R0 = 0x08000000
         * [0x08000000] = 0x20030000
         *
         * 실행 후 R0 = 0x20030000
         *
         * Vector Table의 첫 번째 Word에는
         * Initial MSP(Main Stack Pointer) 값이 저장되어 있다.
         *
         * Vector[0] = Initial MSP
         * Vector[1] = Reset_Handler 주소
         *
         * 따라서 현재 R0에는 초기 MSP 값이 들어 있다.
         */

        " msr msp, r0 \n"
        /*
         * MSP = R0
         *
         * MSR:
         * 일반 목적 레지스터의 값을
         * CPU 특수 레지스터에 기록하는 명령어
         *
         * 예:
         * R0 = 0x20030000
         *
         * 실행 후 MSP = 0x20030000
         *
         * Scheduler를 시작하기 위해
         * MSP를 초기 Stack 위치로 다시 설정한다.
         *
         * MSP는 Handler Mode에서 사용하는 Stack Pointer다.
         *
         * 이후 SVC Handler도 MSP를 사용하여 실행된다.
         */

        " mov r0, #0 \n"
        /*
         * R0 = 0
         *
         * MOV:
         * 상수 또는 레지스터 값을 다른 레지스터에 복사한다.
         *
         * #0:
         * 즉시값(Immediate Value) 0
         *
         * 다음 MSR CONTROL 명령에서 사용하기 위해
         * R0를 0으로 초기화한다.
         */

        " msr control, r0 \n"
        /*
         * CONTROL = 0
         *
         * Cortex-M4F CONTROL 레지스터의 주요 비트:
         *
         * bit[0] nPRIV:
         * 0 = Privileged
         * 1 = Unprivileged
         *
         * bit[1] SPSEL:
         * 0 = Thread Mode에서 MSP 선택
         * 1 = Thread Mode에서 PSP 선택
         *
         * bit[2] FPCA:
         * 0 = Floating Point Context 비활성 상태
         * 1 = Floating Point Context 활성 상태
         *
         * CONTROL = 0이므로:
         *
         * nPRIV = 0 → Privileged 실행
         * SPSEL = 0 → 현재 MSP 사용
         * FPCA  = 0 → FP Context 활성 표시 제거
         *
         * 첫 번째 Task 실행 전에 CPU 상태를 초기화한다.
         *
         * 이후 SVC Handler에서 PSP를 설정하고,
         * EXC_RETURN을 통해 Thread Mode + PSP로 복귀한다.
         */

        " cpsie i \n"
        /*
         * CPSIE I:
         * Change Processor State, Interrupt Enable
         *
         * PRIMASK를 0으로 설정한다.
         *
         * PRIMASK = 0:
         * PRIMASK에 의한 일반 Interrupt 마스킹 해제
         *
         * 단, BASEPRI에 의해 마스킹된 Interrupt까지
         * 모두 해제한다는 의미는 아니다.
         */

        " cpsie f \n"
        /*
         * CPSIE F:
         * FAULTMASK를 0으로 설정한다.
         *
         * FAULTMASK는 HardFault와 NMI를 제외한
         * 여러 Exception의 활성화를 제한할 수 있는
         * 특수 마스킹 레지스터다.
         *
         * 이 명령은 FAULTMASK에 의한 마스킹을 해제한다.
         */

        " dsb \n"
        /*
         * DSB:
         * Data Synchronization Barrier
         *
         * 앞서 실행된 메모리 접근이 완료되도록 보장한 후
         * 이후 명령어 실행을 진행하게 하는 동기화 명령어다.
         *
         * 인터럽트 마스킹 상태 변경 이후
         * 명령 실행 순서를 동기화하기 위해 사용한다.
         */

        " isb \n"
        /*
         * ISB:
         * Instruction Synchronization Barrier
         *
         * 이전에 변경한 CPU 실행 관련 상태가
         * 이후 명령어 실행에 반영되도록
         * 명령어 파이프라인을 동기화한다.
         *
         * CONTROL 등 CPU 상태 변경 이후 사용된다.
         */

        " svc 0 \n"
        /*
         * SVC:
         * Supervisor Call
         *
         * SVC #0 Exception을 발생시킨다.
         *
         * SVC는 Cortex-M의 Exception 번호 11이다.
         * Vector Table에서는 11번 엔트리,
         * 즉 VTOR + 0x2C에 Handler 주소가 저장된다.
         *
         * SVC 발생 시 하드웨어는 현재 Context의
         * R0~R3, R12, LR, PC, xPSR을 Stack에 저장하고
         * Handler Mode로 진입한다.
         *
         * 이 시점에는 MSP를 사용하고 있으므로
         * 해당 기본 Exception Frame은 MSP에 저장된다.
         *
         * FreeRTOS에서는 SVC_Handler가
         * vPortSVCHandler로 연결되어 있어야 한다.
         *
         * vPortSVCHandler는 다음 작업을 수행한다.
         *
         * 1. pxCurrentTCB에서 최초 Task의 TCB 주소를 가져온다.
         * 2. TCB의 pxTopOfStack을 읽는다.
         * 3. R4~R11과 EXC_RETURN을 복원한다.
         * 4. PSP를 최초 Task의 Hardware Frame 위치로 설정한다.
         * 5. BASEPRI를 0으로 설정한다.
         * 6. BX LR로 Exception Return을 수행한다.
         *
         * Exception Return 시 CPU가 PSP에서
         * R0~R3, R12, LR, PC, xPSR을 복원한다.
         *
         * 복원된 PC가 Task 함수 시작 주소이므로
         * 첫 번째 Task가 실행되기 시작한다.
         */

        " nop \n"
        /*
         * NOP:
         * No Operation
         *
         * CPU 상태를 의도적으로 변경하지 않는 명령어다.
         *
         * 정상적으로 첫 Task가 시작되면
         * 이 위치로 실행이 돌아오지 않는다.
         */
    );
}

void vPortSVCHandler(void)
{
    /*
     * SVC Exception 발생 시 실행되는 FreeRTOS 내부 Handler다.
     *
     * prvPortStartFirstTask()에서 SVC #0을 실행하면
     * Cortex-M4F 하드웨어가 SVC Exception을 발생시키고
     * 이 Handler로 진입한다.
     *
     * 주요 역할:
     * 최초 실행할 Task의 TCB에서 Stack Pointer를 가져오고,
     * 소프트웨어로 관리하는 CPU Context를 복원한 다음
     * Exception Return을 수행하는 것이다.
     *
     * SVC Handler는 Handler Mode에서 실행되므로 MSP를 사용한다.
     * 하지만 복원하려는 Task의 Context는 별도의 Task Stack에 있다.
     */

    __asm volatile(
        "   ldr r3, pxCurrentTCBConst2     \n"
        /*
         * R3 = &pxCurrentTCB
         *
         * pxCurrentTCB는 현재 실행 대상으로 선택된
         * Task의 TCB 주소를 저장하는 전역 포인터 변수다.
         *
         * pxCurrentTCBConst2는 함수 아래에 정의된
         * 어셈블리 리터럴 데이터의 레이블이다.
         *
         * 해당 위치에 pxCurrentTCB 변수의 주소가 저장되어 있다.
         *
         * 따라서 이 명령어는 pxCurrentTCB에 저장된
         * TCB 주소가 아니라 pxCurrentTCB 변수 자체의 주소를 읽는다.
         */

        "   ldr r1, [r3]                  \n"
        /*
         * R1 = *(uint32_t *)R3
         *
         * R3에는 &pxCurrentTCB가 들어 있다.
         *
         * [R3]으로 해당 주소의 메모리를 읽으면
         * pxCurrentTCB에 저장된 TCB 주소를 가져온다.
         *
         * 예:
         * R3 = 0x20000100
         * [0x20000100] = 0x20001000
         *
         * 실행 후:
         * R1 = 0x20001000
         *
         * R1은 최초 실행할 Task의 TCB 주소다.
         */

        "   ldr r0, [r1]                  \n"
        /*
         * R0 = *(uint32_t *)R1
         *
         * R1에는 현재 Task의 TCB 주소가 들어 있다.
         *
         * FreeRTOS TCB 구조체의 첫 번째 멤버는
         * pxTopOfStack이므로 TCB 시작 주소에서
         * 첫 번째 Word를 읽으면 저장된 Stack Pointer를 얻는다.
         *
         * 예:
         * R1 = 0x20001000
         * [0x20001000] = 0x20002000
         *
         * 실행 후:
         * R0 = 0x20002000
         *
         * R0는 Task 초기 Context가 저장된 Stack의 시작 위치다.
         *
         * 이 초기 Context는 xTaskCreate() 내부에서
         * pxPortInitialiseStack()이 생성한 것이다.
         */

        "   ldmia r0!, {r4-r11, r14}      \n"
        /*
         * LDMIA:
         * Load Multiple Increment After
         *
         * R0가 가리키는 메모리에서 여러 레지스터 값을 읽는다.
         *
         * 복원 대상:
         * R4, R5, R6, R7, R8, R9, R10, R11, R14
         *
         * R4~R11:
         * Cortex-M Exception 진입 시 하드웨어가 자동 저장하지 않는
         * 일반 목적 레지스터이므로 FreeRTOS Port가 직접 복원한다.
         *
         * R14:
         * LR(Link Register)이다.
         * 최초 Task의 초기 Stack에서는 EXC_RETURN 값인
         * 0xFFFFFFFD를 복원한다.
         *
         * !:
         * Writeback을 의미한다.
         * 여러 레지스터를 읽은 뒤 변경된 주소를 R0에 다시 저장한다.
         *
         * 9개 레지스터 × 4바이트 = 36바이트
         *
         * 예:
         * 실행 전 R0 = 0x20002000
         *
         * R4  = [0x20002000]
         * R5  = [0x20002004]
         * R6  = [0x20002008]
         * R7  = [0x2000200C]
         * R8  = [0x20002010]
         * R9  = [0x20002014]
         * R10 = [0x20002018]
         * R11 = [0x2000201C]
         * R14 = [0x20002020] = 0xFFFFFFFD
         *
         * 실행 후 R0 = 0x20002024
         *
         * R0는 이제 Task의 Hardware Exception Frame 시작 위치를
         * 가리키게 된다.
         *
         * 참고:
         * 이 V10.3.1 ARM_CM4F Port에서 복원하는 것은
         * R4~R11과 EXC_RETURN이다.
         * 일부 다른 Port에서 사용하는 별도의 Critical Nesting 값이
         * 여기서 추가로 복원되는 것은 아니다.
         */

        "   msr psp, r0                   \n"
        /*
         * PSP = R0
         *
         * MSR:
         * 일반 목적 레지스터 값을 CPU 특수 레지스터에 기록한다.
         *
         * PSP:
         * Process Stack Pointer
         *
         * 예:
         * R0 = 0x20002024
         *
         * 실행 후:
         * PSP = 0x20002024
         *
         * 이 주소부터는 Cortex-M4F가 Exception Return 시
         * 자동으로 복원할 Hardware Frame이 저장되어 있다.
         *
         * Hardware Frame:
         * R0, R1, R2, R3, R12, LR, PC, xPSR
         *
         * 현재 Handler 자체는 여전히 MSP를 사용한다.
         * PSP를 설정했다고 즉시 Task로 전환되는 것은 아니다.
         */

        "   isb                            \n"
        /*
         * ISB:
         * Instruction Synchronization Barrier
         *
         * PSP 등의 CPU 상태 변경이 이후 명령어 실행에
         * 올바르게 반영되도록 명령어 흐름을 동기화한다.
         */

        "   mov r0, #0                     \n"
        /*
         * R0 = 0
         *
         * 다음 명령어에서 BASEPRI를 0으로 설정하기 위해
         * R0에 즉시값 0을 저장한다.
         *
         * 이 명령어로 최초 Task의 pvParameters 값이
         * 사라지는 것은 아니다.
         *
         * Task의 초기 R0 값은 PSP가 가리키는 Hardware Frame에
         * 따로 저장되어 있으며 Exception Return 시 복원된다.
         */

        "   msr basepri, r0                \n"
        /*
         * BASEPRI = 0
         *
         * BASEPRI:
         * Base Priority Mask Register
         *
         * FreeRTOS는 Critical Section 등을 처리할 때
         * BASEPRI를 사용해 특정 우선순위 범위의
         * Interrupt를 마스킹한다.
         *
         * BASEPRI = 0으로 설정하면
         * BASEPRI에 의한 Interrupt 우선순위 마스킹이 해제된다.
         *
         * 앞에서 prvPortStartFirstTask()가
         * PRIMASK와 FAULTMASK를 해제했고,
         * 여기서는 BASEPRI를 해제한다.
         *
         * 단, 다른 이유로 비활성화된 개별 Interrupt까지
         * 모두 활성화하는 것은 아니다.
         */

        "   bx r14                          \n"
        /*
         * BX:
         * Branch and Exchange
         *
         * R14는 LR이며 앞의 LDMIA 명령어에서
         * EXC_RETURN = 0xFFFFFFFD로 복원되었다.
         *
         * Cortex-M4F가 Handler Mode에서 유효한
         * EXC_RETURN 값을 이용해 BX를 실행하면
         * 일반 함수 호출 복귀가 아닌 Exception Return을 수행한다.
         *
         * 0xFFFFFFFD의 주요 의미:
         *
         * bit[4] = 1 : Basic Stack Frame 사용
         * bit[3] = 1 : Thread Mode로 복귀
         * bit[2] = 1 : PSP 사용
         *
         * CPU가 PSP의 Hardware Frame에서 다음을 복원한다.
         *
         * R0, R1, R2, R3, R12, LR, PC, xPSR
         *
         * PC:
         * pxPortInitialiseStack()에서 저장한 Task 함수 시작 주소
         *
         * R0:
         * xTaskCreate()에서 전달한 pvParameters
         *
         * xPSR:
         * Thumb 실행 상태 등이 포함된 프로그램 상태 값
         *
         * 복원이 완료되면 CPU는 Thread Mode에서 PSP를 사용하며
         * 최초 Task 함수의 첫 명령어부터 실행하기 시작한다.
         *
         * 따라서 BX R14는 첫 Task 실행으로 넘어가는
         * 결정적인 명령어다.
         */

        "                                   \n"

        "   .align 4                        \n"
        /*
         * 어셈블러 정렬 지시문
         *
         * GNU assembler의 ARM 환경에서 .align 4는
         * 다음 데이터 위치를 2^4 = 16바이트 경계에 정렬한다.
         *
         * CPU가 실행하는 명령어가 아니라
         * 어셈블 과정에서 적용되는 지시문이다.
         */

        "pxCurrentTCBConst2: .word pxCurrentTCB \n"
        /*
         * pxCurrentTCBConst2라는 레이블에
         * pxCurrentTCB 변수의 주소를 32비트 Word로 저장한다.
         *
         * 예:
         * &pxCurrentTCB = 0x20000100
         *
         * pxCurrentTCBConst2 위치에 0x20000100을 저장한다.
         *
         * 앞의 첫 번째 명령어:
         *
         * LDR R3, pxCurrentTCBConst2
         *
         * 가 이 Word를 읽어서 R3에
         * pxCurrentTCB 변수의 주소를 가져온다.
         *
         * 주의:
         * 이 위치에 저장된 것은 TCB의 주소가 아니라
         * pxCurrentTCB라는 전역 포인터 변수의 주소다.
         */
    );
}

int main(void)
{
    pxPortInitialiseStack();
    vTaskStartScheduler();
    xPortStartScheduler();
    vPortSetupTimerInterrupt();
    prvPortStartFirstTask();
    vPortSVCHandler();
}
