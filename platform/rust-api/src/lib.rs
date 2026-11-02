#![no_std]
use core::panic::PanicInfo;

unsafe extern "C" {
    fn rtos_delay_ms(ms: u32);
    fn rtos_led_toggle();
    fn rtos_uart_write(data: *const u8, len: usize);
    fn rtos_panic() -> !;
}

/// Owns the single application task's LED and UART access.
/// The token cannot be moved to another thread or shared across threads.
pub struct BoardTask {
    _not_send_sync: core::marker::PhantomData<*mut ()>,
}
impl BoardTask {
    /// # Safety
    /// Call once, inside the sole FreeRTOS application task after board_init.
    /// No C/ISR/other task may access LED/UART concurrently.
    pub unsafe fn claim() -> Self {
        Self {
            _not_send_sync: core::marker::PhantomData,
        }
    }
    pub fn delay_ms(&mut self, ms: u32) {
        // SAFETY: The token is created only in a running FreeRTOS task.
        // Clamping avoids tick conversion overflow if the API is reused.
        unsafe { rtos_delay_ms(ms.clamp(1, 60_000)) }
    }
    pub fn toggle_led(&mut self) {
        // SAFETY: This token exclusively owns PA5 and requires mutable access.
        unsafe { rtos_led_toggle() }
    }
    pub fn write(&mut self, message: &str) {
        // SAFETY: The slice is readable for len bytes throughout this synchronous
        // call. C neither retains the pointer nor writes through it. usize=size_t.
        unsafe { rtos_uart_write(message.as_ptr(), message.len()) }
    }
}
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // SAFETY: This function disables interrupts, records reason 5, never returns,
    // and does not allocate or access UART. No unwind crosses C.
    unsafe { rtos_panic() }
}
