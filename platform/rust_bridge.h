#ifndef RUST_BRIDGE_H
#define RUST_BRIDGE_H
#include <stdint.h>
#include <stddef.h>
/* Task context only. Caller owns LED/UART. data is readable for len bytes,
   used synchronously and never retained. Rust must not unwind across this ABI. */
void rtos_delay_ms(uint32_t ms);
void rtos_led_toggle(void);
void rtos_uart_write(const uint8_t *data, size_t len);
_Noreturn void rtos_panic(void);
#endif
