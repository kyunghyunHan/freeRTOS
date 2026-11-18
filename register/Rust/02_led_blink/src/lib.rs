#![no_std]
#![no_main]

#[path = "../../support/startup.rs"]
mod startup;

use core::panic::PanicInfo;
use core::ptr::{read_volatile, write_volatile};

// NUCLEO-F429ZI: LD1=PB0, ST-LINK VCP=USART3 PD8/PD9
const RCC_AHB1ENR: *mut u32 = 0x4002_3830 as *mut u32;
const GPIOB_MODER: *mut u32 = 0x4002_0400 as *mut u32;
const GPIOB_OTYPER: *mut u32 = 0x4002_0404 as *mut u32;
const GPIOB_PUPDR: *mut u32 = 0x4002_040c as *mut u32;
const GPIOB_ODR: *mut u32 = 0x4002_0414 as *mut u32;
const GPIOB_BSRR: *mut u32 = 0x4002_0418 as *mut u32;
const GPIOD_MODER: *mut u32 = 0x4002_0c00 as *mut u32;
const GPIOD_OTYPER: *mut u32 = 0x4002_0c04 as *mut u32;
const GPIOD_PUPDR: *mut u32 = 0x4002_0c0c as *mut u32;
const GPIOD_AFRH: *mut u32 = 0x4002_0c24 as *mut u32;
const RCC_APB1ENR: *mut u32 = 0x4002_3840 as *mut u32;
const USART3_SR: *mut u32 = 0x4000_4800 as *mut u32;
const USART3_DR: *mut u32 = 0x4000_4804 as *mut u32;
const USART3_BRR: *mut u32 = 0x4000_4808 as *mut u32;
const USART3_CR1: *mut u32 = 0x4000_480c as *mut u32;
const USART3_CR2: *mut u32 = 0x4000_4810 as *mut u32;
const USART3_CR3: *mut u32 = 0x4000_4814 as *mut u32;

unsafe extern "C" {
    // FreeRTOS 지연: 32비트 tick, 1tick = 1ms
    fn vTaskDelay(ticks: u32);
    fn firmware_fail(reason: u32) -> !;
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // panic 오류 기록 후 정지
    unsafe { firmware_fail(5) }
}

#[unsafe(no_mangle)]
pub extern "C" fn main() -> ! {
    startup::start(rust_task)
}

// Rust main이 생성하는 FreeRTOS 태스크
#[unsafe(no_mangle)]
pub extern "C" fn rust_task(_: *mut core::ffi::c_void) {
    // HSI 16MHz에서 단일 태스크가 레지스터 직접 제어
    unsafe {
        write_volatile(
            RCC_AHB1ENR,
            read_volatile(RCC_AHB1ENR) | (1 << 1) | (1 << 3),
        );
        let _ = read_volatile(RCC_AHB1ENR); // 클록 활성화 확인
        write_volatile(GPIOB_BSRR, 1 << 16); // LD1 초기 꺼짐
        write_volatile(GPIOB_MODER, (read_volatile(GPIOB_MODER) & !3) | 1);
        write_volatile(GPIOB_OTYPER, read_volatile(GPIOB_OTYPER) & !1);
        write_volatile(GPIOB_PUPDR, read_volatile(GPIOB_PUPDR) & !3);
        // PD8/PD9 USART3 AF7: 115200 baud
        write_volatile(
            GPIOD_MODER,
            (read_volatile(GPIOD_MODER) & !((3 << 16) | (3 << 18))) | (2 << 16) | (2 << 18),
        );
        write_volatile(
            GPIOD_OTYPER,
            read_volatile(GPIOD_OTYPER) & !((1 << 8) | (1 << 9)),
        );
        write_volatile(
            GPIOD_PUPDR,
            (read_volatile(GPIOD_PUPDR) & !((3 << 16) | (3 << 18))) | (1 << 18),
        );
        write_volatile(GPIOD_AFRH, (read_volatile(GPIOD_AFRH) & !0xff) | 0x77);
        write_volatile(RCC_APB1ENR, read_volatile(RCC_APB1ENR) | (1 << 18));
        let _ = read_volatile(RCC_APB1ENR);
        write_volatile(USART3_CR1, 0);
        write_volatile(USART3_CR2, 0);
        write_volatile(USART3_CR3, 0);
        write_volatile(USART3_BRR, 139);
        write_volatile(USART3_CR1, (1 << 13) | (1 << 3) | (1 << 2));
        for &byte in b"Rust FreeRTOS: PB0 toggles every 1000 ms\r\n" {
            while read_volatile(USART3_SR) & (1 << 7) == 0 {}
            write_volatile(USART3_DR, u32::from(byte));
        }
        loop {
            let reset_or_set = if read_volatile(GPIOB_ODR) & 1 != 0 {
                1 << 16
            } else {
                1
            };
            write_volatile(GPIOB_BSRR, reset_or_set);
            vTaskDelay(1000);
        }
    }
}
