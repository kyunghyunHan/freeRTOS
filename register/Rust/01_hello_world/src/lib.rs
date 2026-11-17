#![no_std]

use core::panic::PanicInfo;
use core::ptr::{read_volatile, write_volatile};

// STM32F446 memory map. Each constant names the actual 32-bit register.
const RCC_AHB1ENR: *mut u32 = 0x4002_3830 as *mut u32;
const GPIOA_MODER: *mut u32 = 0x4002_0000 as *mut u32;
const GPIOA_OTYPER: *mut u32 = 0x4002_0004 as *mut u32;
const GPIOA_PUPDR: *mut u32 = 0x4002_000c as *mut u32;
const GPIOA_AFRL: *mut u32 = 0x4002_0020 as *mut u32;
const RCC_APB1ENR: *mut u32 = 0x4002_3840 as *mut u32;
const USART2_SR: *mut u32 = 0x4000_4400 as *mut u32;
const USART2_DR: *mut u32 = 0x4000_4404 as *mut u32;
const USART2_BRR: *mut u32 = 0x4000_4408 as *mut u32;
const USART2_CR1: *mut u32 = 0x4000_440c as *mut u32;
const USART2_CR2: *mut u32 = 0x4000_4410 as *mut u32;
const USART2_CR3: *mut u32 = 0x4000_4414 as *mut u32;

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
        // PA2/PA3: AF7, USART2 TX/RX, RX pull-up; 115200 baud at 16 MHz.
        write_volatile(
            GPIOA_MODER,
            (read_volatile(GPIOA_MODER) & !((3 << 4) | (3 << 6))) | (2 << 4) | (2 << 6),
        );
        write_volatile(
            GPIOA_OTYPER,
            read_volatile(GPIOA_OTYPER) & !((1 << 2) | (1 << 3)),
        );
        write_volatile(
            GPIOA_PUPDR,
            (read_volatile(GPIOA_PUPDR) & !((3 << 4) | (3 << 6))) | (1 << 6),
        );
        write_volatile(
            GPIOA_AFRL,
            (read_volatile(GPIOA_AFRL) & !((15 << 8) | (15 << 12))) | (7 << 8) | (7 << 12),
        );
        write_volatile(RCC_APB1ENR, read_volatile(RCC_APB1ENR) | (1 << 17));
        let _ = read_volatile(RCC_APB1ENR);
        write_volatile(USART2_CR1, 0);
        write_volatile(USART2_CR2, 0);
        write_volatile(USART2_CR3, 0);
        write_volatile(USART2_BRR, 139);
        write_volatile(USART2_CR1, (1 << 13) | (1 << 3) | (1 << 2));
        loop {
            for &byte in b"Hello from Rust in a FreeRTOS task!\r\n" {
                while read_volatile(USART2_SR) & (1 << 7) == 0 {}
                write_volatile(USART2_DR, u32::from(byte));
            }
            vTaskDelay(1000);
        }
    }
}
