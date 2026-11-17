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
const GPIOC_MODER: *mut u32 = 0x4002_0800 as *mut u32;
const GPIOC_PUPDR: *mut u32 = 0x4002_080c as *mut u32;
const GPIOC_IDR: *mut u32 = 0x4002_0810 as *mut u32;

const POLL_MS: u32 = 10;
const DEBOUNCE_MS: u32 = 30;

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
            // 버튼 눌림 확정 시 LED 토글
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
