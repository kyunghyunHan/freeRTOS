use core::{
    ffi::{c_char, c_void},
    ptr::{read_volatile, write_volatile},
};

unsafe extern "C" {
    fn SystemCoreClockUpdate();
    fn xTaskCreate(
        entry: extern "C" fn(*mut c_void),
        name: *const c_char,
        stack_words: u32,
        argument: *mut c_void,
        priority: u32,
        handle: *mut *mut c_void,
    ) -> i32;
    fn vTaskStartScheduler();
    fn firmware_fail(reason: u32) -> !;
}

// Rust owns application startup, task creation, and scheduler startup.
pub fn start(entry: extern "C" fn(*mut c_void)) -> ! {
    const RCC_CR: *mut u32 = 0x4002_3800 as *mut u32;
    const RCC_CFGR: *mut u32 = 0x4002_3808 as *mut u32;
    unsafe {
        write_volatile(RCC_CR, read_volatile(RCC_CR) | 1);
        while read_volatile(RCC_CR) & (1 << 1) == 0 {}
        write_volatile(RCC_CFGR, 0);
        while read_volatile(RCC_CFGR) & (3 << 2) != 0 {}
        write_volatile(RCC_CR, read_volatile(RCC_CR) & !(1 << 24));
        SystemCoreClockUpdate();
        // 512 32-bit words = 2048 bytes of FreeRTOS task stack.
        if xTaskCreate(
            entry,
            c"rust".as_ptr(),
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
