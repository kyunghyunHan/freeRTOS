#![no_std]

use core::panic::PanicInfo;
use core::ptr::{read_volatile, write_volatile};

// STM32F446 memory map. Each constant names the actual 32-bit register.
const RCC_AHB1ENR: *mut u32 = 0x4002_3830 as *mut u32;
const GPIOA_MODER: *mut u32 = 0x4002_0000 as *mut u32;
const GPIOA_OTYPER: *mut u32 = 0x4002_0004 as *mut u32;
const GPIOA_PUPDR: *mut u32 = 0x4002_000c as *mut u32;
const GPIOA_ODR: *mut u32 = 0x4002_0014 as *mut u32;
const GPIOA_BSRR: *mut u32 = 0x4002_0018 as *mut u32;
const GPIOC_MODER: *mut u32 = 0x4002_0800 as *mut u32;
const GPIOC_PUPDR: *mut u32 = 0x4002_080c as *mut u32;
const GPIOC_IDR: *mut u32 = 0x4002_0810 as *mut u32;

const POLL_MS: u32 = 10;
const DEBOUNCE_MS: u32 = 30;

unsafe extern "C" {
    // FreeRTOSConfig.h selects 32-bit ticks at 1000 Hz: one tick = one ms.
    fn vTaskDelay(ticks: u32);
    fn firmware_fail(reason: u32) -> !;
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // SAFETY: Disables interrupts, records reason 5, and never returns/unwinds.
    unsafe { firmware_fail(5) }
}

// SAFETY: C creates exactly one application task and calls this entry once.
#[unsafe(no_mangle)]
pub extern "C" fn rust_task() -> ! {
    // SAFETY: These aligned addresses are STM32F446 peripheral registers.
    // This task exclusively owns the GPIO/UART; no peripheral ISR accesses them.
    // C main has selected HSI 16 MHz, APB /1 before starting the scheduler.
    unsafe {
        write_volatile(RCC_AHB1ENR, read_volatile(RCC_AHB1ENR) | 1);
        let _ = read_volatile(RCC_AHB1ENR); // Clock-enable readback.
        write_volatile(GPIOA_BSRR, 1 << 21); // PA5 initially LOW.
        write_volatile(
            GPIOA_MODER,
            (read_volatile(GPIOA_MODER) & !(3 << 10)) | (1 << 10),
        );
        write_volatile(GPIOA_OTYPER, read_volatile(GPIOA_OTYPER) & !(1 << 5));
        write_volatile(GPIOA_PUPDR, read_volatile(GPIOA_PUPDR) & !(3 << 10));
        write_volatile(RCC_AHB1ENR, read_volatile(RCC_AHB1ENR) | (1 << 2));
        let _ = read_volatile(RCC_AHB1ENR);
        write_volatile(GPIOC_MODER, read_volatile(GPIOC_MODER) & !(3 << 26));
        write_volatile(
            GPIOC_PUPDR,
            (read_volatile(GPIOC_PUPDR) & !(3 << 26)) | (1 << 26),
        );
        let mut stable_pressed = false;
        let mut candidate_pressed = false;
        let mut unchanged_ms = 0;
        loop {
            let pressed = read_volatile(GPIOC_IDR) & (1 << 13) == 0;
            if pressed != candidate_pressed {
                candidate_pressed = pressed;
                unchanged_ms = 0;
            } else if unchanged_ms < DEBOUNCE_MS {
                unchanged_ms += POLL_MS;
            }
            // Debounce both edges; toggle only on a new stable press.
            if unchanged_ms >= DEBOUNCE_MS && stable_pressed != candidate_pressed {
                stable_pressed = candidate_pressed;
                if stable_pressed {
                    let reset_or_set = if read_volatile(GPIOA_ODR) & (1 << 5) != 0 {
                        1 << 21
                    } else {
                        1 << 5
                    };
                    write_volatile(GPIOA_BSRR, reset_or_set);
                }
            }
            vTaskDelay(POLL_MS);
        }
    }
}
