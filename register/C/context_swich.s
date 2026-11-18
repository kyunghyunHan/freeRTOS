
.syntax unified
.cpu cortex-m4
.thumb

/*
 * Cortex-M4 Context Switching
 *
 * SVC:
 *   첫 Task 실행
 *
 * PendSV:
 *   현재 Task Context 저장
 *   Scheduler 실행
 *   다음 Task Context 복원
 *
 * FPU Context는 처리하지 않음.
 */

.extern current_task
.extern Scheduler_SelectNext

// ============================================
// 1. SVC Handler
// ============================================

.global SVC_Handler
.type SVC_Handler, %function
.thumb_func

SVC_Handler:

    // R0 = &current_task
    ldr r0, =current_task

    // R0 = current_task
    ldr r0, [r0]

    // R0 = current_task->sp
    ldr r0, [r0]

    // 최초 Task R4-R11 복원
    ldmia r0!, {r4-r11}

    // PSP = Hardware Stack Frame 시작 주소
    msr psp, r0
    isb

    // Exception Return:
    // Thread Mode + PSP + Basic Frame
    ldr lr, =0xFFFFFFFD

    // CPU가 R0-R3, R12, LR, PC, xPSR 복원
    bx lr

.size SVC_Handler, .-SVC_Handler


// ============================================
// 2. PendSV Handler
// ============================================

.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func

PendSV_Handler:

    // ----------------------------------------
    // STEP 1. 현재 Task PSP 읽기
    // ----------------------------------------

    mrs r0, psp
    isb

    // ----------------------------------------
    // STEP 2. R4-R11 저장
    // ----------------------------------------

    stmdb r0!, {r4-r11}

    // ----------------------------------------
    // STEP 3. 현재 TCB에 PSP 저장
    // ----------------------------------------

    ldr r1, =current_task

    // R2 = current_task
    ldr r2, [r1]

    // current_task->sp = R0
    str r0, [r2]

    // ----------------------------------------
    // STEP 4. Exception Return 값 보존
    // ----------------------------------------

    // Handler Mode에서는 MSP 사용
    push {r3, lr}

    // ----------------------------------------
    // STEP 5. Scheduler 실행
    // ----------------------------------------

    bl Scheduler_SelectNext

    // ----------------------------------------
    // STEP 6. Exception Return 값 복구
    // ----------------------------------------

    pop {r3, lr}

    // ----------------------------------------
    // STEP 7. 다음 TCB 선택
    // ----------------------------------------

    ldr r1, =current_task

    // R2 = Next TCB
    ldr r2, [r1]

    // R0 = Next TCB->sp
    ldr r0, [r2]

    // ----------------------------------------
    // STEP 8. 다음 Task R4-R11 복원
    // ----------------------------------------

    ldmia r0!, {r4-r11}

    // ----------------------------------------
    // STEP 9. 다음 Task PSP 복원
    // ----------------------------------------

    msr psp, r0
    isb

    // ----------------------------------------
    // STEP 10. Exception Return
    // ----------------------------------------

    // CPU가 남은 Context를 자동 복원
    bx lr

.size PendSV_Handler, .-PendSV_Handler
