#![no_std]
#![no_main]

use core::{
    ffi::{c_char, c_void},
    panic::PanicInfo,
    ptr::{read_volatile, write_volatile},
};

const RCC_AHB1ENR: usize = 0x4002_3830;
const RCC_APB1ENR: usize = 0x4002_3840;
const GPIOD_MODER: usize = 0x4002_0c00;
const GPIOD_OTYPER: usize = 0x4002_0c04;
const GPIOD_PUPDR: usize = 0x4002_0c0c;
const GPIOD_AFRH: usize = 0x4002_0c24;
const USART3_SR: usize = 0x4000_4800;
const USART3_DR: usize = 0x4000_4804;
const USART3_BRR: usize = 0x4000_4808;
const USART3_CR1: usize = 0x4000_480c;
const USART3_CR2: usize = 0x4000_4810;
const USART3_CR3: usize = 0x4000_4814;

unsafe extern "C" {
    fn xTaskCreate(
        entry: extern "C" fn(*mut c_void),
        name: *const c_char,
        stack_words: u32,
        argument: *mut c_void,
        priority: u32,
        handle: *mut *mut c_void,
    ) -> i32;
    fn vTaskStartScheduler();
    fn vTaskDelay(ticks: u32);
    fn firmware_fail(reason: u32) -> !;
}

// Only the Hello task accesses these peripherals.
unsafe fn read(address: usize) -> u32 {
    unsafe { read_volatile(address as *const u32) }
}

unsafe fn write(address: usize, value: u32) {
    unsafe { write_volatile(address as *mut u32, value) }
}

extern "C" fn hello_task(_: *mut c_void) {
    unsafe {
        // NUCLEO-F429ZI ST-LINK VCP: USART3 TX=PD8, RX=PD9, AF7.
        write(RCC_AHB1ENR, read(RCC_AHB1ENR) | (1 << 3));
        let _ = read(RCC_AHB1ENR);
        write(
            GPIOD_MODER,
            (read(GPIOD_MODER) & !((3 << 16) | (3 << 18))) | (2 << 16) | (2 << 18),
        );
        write(GPIOD_OTYPER, read(GPIOD_OTYPER) & !((1 << 8) | (1 << 9)));
        write(
            GPIOD_PUPDR,
            (read(GPIOD_PUPDR) & !((3 << 16) | (3 << 18))) | (1 << 18),
        );
        write(GPIOD_AFRH, (read(GPIOD_AFRH) & !0xff) | 0x77);
        write(RCC_APB1ENR, read(RCC_APB1ENR) | (1 << 18));
        let _ = read(RCC_APB1ENR);
        // CMSIS SystemInit leaves the MCU on HSI 16 MHz, APB1 /1.
        write(USART3_CR1, 0);
        write(USART3_CR2, 0);
        write(USART3_CR3, 0);
        write(USART3_BRR, 139); // 16 MHz / 115200, rounded; 8N1.
        write(USART3_CR1, (1 << 13) | (1 << 3) | (1 << 2));
        loop {
            for &byte in b"Hello World from Rust + FreeRTOS!\r\n" {
                while read(USART3_SR) & (1 << 7) == 0 {}
                write(USART3_DR, u32::from(byte));
            }
            vTaskDelay(1000); // configTICK_RATE_HZ = 1000: one second.
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn main() -> ! {
    unsafe {
        if xTaskCreate(
            hello_task,
            c"hello".as_ptr(),
            512,
            core::ptr::null_mut(),
            1,
            core::ptr::null_mut(),
        ) != 1
        {
            firmware_fail(7);
        }
        vTaskStartScheduler();
        firmware_fail(6);
    }
}

#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    unsafe { firmware_fail(5) }
}
