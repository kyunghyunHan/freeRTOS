#![no_std]

use core::panic::PanicInfo;
use core::ptr::{read_volatile, write_volatile};

// STM32F446 레지스터 주소
const RCC_AHB1ENR: *mut u32 = 0x4002_3830 as *mut u32;
const GPIOA_MODER: *mut u32 = 0x4002_0000 as *mut u32;
const GPIOA_OTYPER: *mut u32 = 0x4002_0004 as *mut u32;
const GPIOA_PUPDR: *mut u32 = 0x4002_000c as *mut u32;
const GPIOA_ODR: *mut u32 = 0x4002_0014 as *mut u32;
const GPIOA_BSRR: *mut u32 = 0x4002_0018 as *mut u32;
const GPIOA_AFRL: *mut u32 = 0x4002_0020 as *mut u32;
const RCC_APB1ENR: *mut u32 = 0x4002_3840 as *mut u32;
const USART2_SR: *mut u32 = 0x4000_4400 as *mut u32;
const USART2_DR: *mut u32 = 0x4000_4404 as *mut u32;
const USART2_BRR: *mut u32 = 0x4000_4408 as *mut u32;
const USART2_CR1: *mut u32 = 0x4000_440c as *mut u32;
const USART2_CR2: *mut u32 = 0x4000_4410 as *mut u32;
const USART2_CR3: *mut u32 = 0x4000_4414 as *mut u32;

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

// C에서 한 번 시작하는 태스크
#[unsafe(no_mangle)]
pub extern "C" fn rust_task() -> ! {
    // HSI 16MHz에서 단일 태스크가 레지스터 직접 제어
    unsafe {
        write_volatile(RCC_AHB1ENR, read_volatile(RCC_AHB1ENR) | 1);
        let _ = read_volatile(RCC_AHB1ENR); // 클록 활성화 확인
        write_volatile(GPIOA_BSRR, 1 << 21); // LED 초기 꺼짐
        write_volatile(
            GPIOA_MODER,
            (read_volatile(GPIOA_MODER) & !(3 << 10)) | (1 << 10),
        );
        write_volatile(GPIOA_OTYPER, read_volatile(GPIOA_OTYPER) & !(1 << 5));
        write_volatile(GPIOA_PUPDR, read_volatile(GPIOA_PUPDR) & !(3 << 10));
        // PA2/PA3 USART2 설정: 115200 baud
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
        for &byte in b"Rust FreeRTOS: PA5 toggles every 1000 ms\r\n" {
            while read_volatile(USART2_SR) & (1 << 7) == 0 {}
            write_volatile(USART2_DR, u32::from(byte));
        }
        loop {
            let reset_or_set = if read_volatile(GPIOA_ODR) & (1 << 5) != 0 {
                1 << 21
            } else {
                1 << 5
            };
            write_volatile(GPIOA_BSRR, reset_or_set);
            vTaskDelay(1000);
        }
    }
}
