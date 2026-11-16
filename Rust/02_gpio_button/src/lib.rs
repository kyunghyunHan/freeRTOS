#![no_std]
use freertos_board::BoardTask;

const POLL_MS: u32 = 10;
const DEBOUNCE_MS: u32 = 30;

// SAFETY: One globally unique symbol with the C ABI expected by rust_entry.c.
#[unsafe(no_mangle)]
pub extern "C" fn rust_task() -> ! {
    // SAFETY: The C bootstrap calls this once from its sole application task,
    // after board_init; no ISR or other task owns the LED/UART.
    let mut board = unsafe { BoardTask::claim() };
    let mut stable_pressed = false;
    let mut candidate_pressed = false;
    let mut unchanged_ms = 0;

    loop {
        let pressed = board.button_pressed();
        if pressed != candidate_pressed {
            candidate_pressed = pressed;
            unchanged_ms = 0;
        } else if unchanged_ms < DEBOUNCE_MS {
            unchanged_ms += POLL_MS;
        }

        // Debounce both edges; only a new stable press toggles the LED.
        if unchanged_ms >= DEBOUNCE_MS && stable_pressed != candidate_pressed {
            stable_pressed = candidate_pressed;
            if stable_pressed {
                board.toggle_led();
            }
        }
        board.delay_ms(POLL_MS);
    }
}
