
#![no_std]
#![no_main]

use core::ffi::{c_char, c_void};
use core::panic::PanicInfo;
use core::ptr::{read_volatile, write_volatile};

/*
 * STM32F429ZI
 *
 * Rust + FreeRTOS + Register
 * HAL 사용하지 않음
 *
 * LD1 : PB0  - 500ms마다 ON/OFF
 * LD2 : PB7  - B1 버튼으로 제어
 * LD3 : PB14 - 항상 ON
 * B1  : PC13 - USER Button
 *
 * FreeRTOS Task 2개 사용
 *
 * 주의:
 * FreeRTOS Kernel, Startup, Linker Script는
 * 기존 프로젝트에 연결되어 있어야 함.
 */

// =========================================
// 1. RCC Register
// =========================================

const RCC_BASE: usize = 0x4002_3800;
const RCC_AHB1ENR: usize = RCC_BASE + 0x30;

// =========================================
// 2. GPIOB - LED
// =========================================

const GPIOB_BASE: usize = 0x4002_0400;

const GPIOB_MODER: usize  = GPIOB_BASE + 0x00;
const GPIOB_OTYPER: usize = GPIOB_BASE + 0x04;
const GPIOB_PUPDR: usize  = GPIOB_BASE + 0x0C;
const GPIOB_BSRR: usize   = GPIOB_BASE + 0x18;

const LD1_PIN: u32 = 0;
const LD2_PIN: u32 = 7;
const LD3_PIN: u32 = 14;

// =========================================
// 3. GPIOC - Button
// =========================================

const GPIOC_BASE: usize = 0x4002_0800;

const GPIOC_MODER: usize = GPIOC_BASE + 0x00;
const GPIOC_PUPDR: usize = GPIOC_BASE + 0x0C;
const GPIOC_IDR: usize   = GPIOC_BASE + 0x10;

const BUTTON_PIN: u32 = 13;

// =========================================
// 4. FreeRTOS 설정
// =========================================

// 실제 FreeRTOSConfig.h의 configTICK_RATE_HZ와 일치
const TICK_RATE_HZ: u32 = 1000;

const TASK_STACK_DEPTH: u32 = 256;
const TASK_PRIORITY: u32 = 1;

const PD_PASS: i32 = 1;

// =========================================
// 5. FreeRTOS C API - FFI
// =========================================

unsafe extern "C" {
    fn xTaskCreate(
        task_code: extern "C" fn(*mut c_void),
        name: *const c_char,
        stack_depth: u32,
        parameters: *mut c_void,
        priority: u32,
        task_handle: *mut *mut c_void,
    ) -> i32;

    fn vTaskDelay(ticks: u32);

    fn vTaskStartScheduler();
}

// =========================================
// 6. Register Access
// =========================================

#[inline(always)]
unsafe fn reg_read(address: usize) -> u32 {
    unsafe {
        read_volatile(address as *const u32)
    }
}

#[inline(always)]
unsafe fn reg_write(address: usize, value: u32) {
    unsafe {
        write_volatile(address as *mut u32, value);
    }
}

// =========================================
// 7. GPIO Initialization
// =========================================

fn gpio_init() {
    unsafe {
        // GPIOB / GPIOC Clock Enable
        let value = reg_read(RCC_AHB1ENR);

        reg_write(
            RCC_AHB1ENR,
            value | (1 << 1) | (1 << 2),
        );

        // Clock Enable 반영 확인
        let _ = reg_read(RCC_AHB1ENR);

        // ---------------------------------
        // PB0, PB7, PB14 Output Mode (01)
        // ---------------------------------

        let mut moder = reg_read(GPIOB_MODER);

        moder &= !(
            (3 << (LD1_PIN * 2)) |
            (3 << (LD2_PIN * 2)) |
            (3 << (LD3_PIN * 2))
        );

        moder |=
            (1 << (LD1_PIN * 2)) |
            (1 << (LD2_PIN * 2)) |
            (1 << (LD3_PIN * 2));

        reg_write(GPIOB_MODER, moder);

        // ---------------------------------
        // PB0, PB7, PB14 Push-Pull
        // ---------------------------------

        let otyper = reg_read(GPIOB_OTYPER);

        reg_write(
            GPIOB_OTYPER,
            otyper & !(
                (1 << LD1_PIN) |
                (1 << LD2_PIN) |
                (1 << LD3_PIN)
            ),
        );

        // ---------------------------------
        // PB0, PB7, PB14 No Pull-up/down
        // ---------------------------------

        let pupdr = reg_read(GPIOB_PUPDR);

        reg_write(
            GPIOB_PUPDR,
            pupdr & !(
                (3 << (LD1_PIN * 2)) |
                (3 << (LD2_PIN * 2)) |
                (3 << (LD3_PIN * 2))
            ),
        );

        // ---------------------------------
        // PC13 Input Mode
        // ---------------------------------

        let moder = reg_read(GPIOC_MODER);

        reg_write(
            GPIOC_MODER,
            moder & !(3 << (BUTTON_PIN * 2)),
        );

        // PC13 No Internal Pull-up/down
        let pupdr = reg_read(GPIOC_PUPDR);

        reg_write(
            GPIOC_PUPDR,
            pupdr & !(3 << (BUTTON_PIN * 2)),
        );

        // ---------------------------------
        // Initial LED State
        // ---------------------------------

        // LD1, LD2 OFF
        reg_write(
            GPIOB_BSRR,
            (1 << (LD1_PIN + 16)) |
            (1 << (LD2_PIN + 16)),
        );

        // LD3 ON (PB14 HIGH)
        reg_write(
            GPIOB_BSRR,
            1 << LD3_PIN,
        );
    }
}

// =========================================
// 8. FreeRTOS Delay
// =========================================

fn delay_ms(ms: u32) {
    let ticks = (
        (ms as u64 * TICK_RATE_HZ as u64)
            / 1000
    ) as u32;

    unsafe {
        vTaskDelay(ticks.max(1));
    }
}

// =========================================
// 9. Task 1 - Button Task
// =========================================

extern "C" fn button_task(_argument: *mut c_void) {
    loop {
        unsafe {
            // PC13 버튼 읽기
            let button = reg_read(GPIOC_IDR);

            if (button & (1 << BUTTON_PIN)) != 0 {
                // 버튼 누름 -> LD2 ON
                reg_write(
                    GPIOB_BSRR,
                    1 << LD2_PIN,
                );
            } else {
                // 버튼 뗌 -> LD2 OFF
                reg_write(
                    GPIOB_BSRR,
                    1 << (LD2_PIN + 16),
                );
            }
        }

        // 10ms 동안 Blocked
        delay_ms(10);
    }
}

// =========================================
// 10. Task 2 - Blink Task
// =========================================

extern "C" fn blink_task(_argument: *mut c_void) {
    let mut led_on = false;

    loop {
        // LD1 상태 반전
        led_on = !led_on;

        unsafe {
            if led_on {
                // LD1 ON
                reg_write(
                    GPIOB_BSRR,
                    1 << LD1_PIN,
                );
            } else {
                // LD1 OFF
                reg_write(
                    GPIOB_BSRR,
                    1 << (LD1_PIN + 16),
                );
            }
        }

        // 500ms 동안 Blocked
        delay_ms(500);
    }
}

// =========================================
// 11. Main
// =========================================

#[unsafe(no_mangle)]
pub extern "C" fn main() -> i32 {
    // GPIO 초기화
    gpio_init();

    unsafe {
        // Task 1 생성
        let result1 = xTaskCreate(
            button_task,
            c"Button_Task".as_ptr(),
            TASK_STACK_DEPTH,
            core::ptr::null_mut(),
            TASK_PRIORITY,
            core::ptr::null_mut(),
        );

        // Task 2 생성
        let result2 = xTaskCreate(
            blink_task,
            c"Blink_Task".as_ptr(),
            TASK_STACK_DEPTH,
            core::ptr::null_mut(),
            TASK_PRIORITY,
            core::ptr::null_mut(),
        );

        // Task 생성 실패 검사
        if result1 != PD_PASS || result2 != PD_PASS {
            loop {
                core::hint::spin_loop();
            }
        }

        // FreeRTOS Scheduler 시작
        vTaskStartScheduler();
    }

    // 정상적으로는 도달하지 않음
    loop {
        core::hint::spin_loop();
    }
}

// =========================================
// 12. Panic Handler
// =========================================

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        core::hint::spin_loop();
    }
}
