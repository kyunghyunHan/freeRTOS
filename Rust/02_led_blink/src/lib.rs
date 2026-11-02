#![no_std]
use freertos_board::BoardTask;

// SAFETY: One globally unique symbol with the C ABI expected by rust_entry.c.
#[unsafe(no_mangle)]
pub extern "C" fn rust_task() -> ! {
    // SAFETY: The C bootstrap calls this once from its sole application task,
    // after board_init; no ISR or other task owns the LED/UART.
    let mut board = unsafe { BoardTask::claim() };
    board.write("Rust FreeRTOS: PA5 toggles every 1000 ms\r\n");
    loop {
        board.toggle_led();
        board.delay_ms(1000);
    }
}
