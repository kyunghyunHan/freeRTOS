#![no_std]
use freertos_board::BoardTask;

// SAFETY: One globally unique symbol with the C ABI expected by rust_entry.c.
#[unsafe(no_mangle)]
pub extern "C" fn rust_task() -> ! {
    // SAFETY: The C bootstrap calls this once from its sole application task,
    // after board_init; no ISR or other task owns the LED/UART.
    let mut board = unsafe { BoardTask::claim() };
    loop {
        board.write("Hello from Rust in a FreeRTOS task!\r\n");
        board.delay_ms(1000);
    }
}
