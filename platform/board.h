#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stddef.h>
void board_init(void);
void board_led_toggle(void);
void board_uart_write(const uint8_t *data, size_t len);
/* Stops with interrupts disabled. Inspect g_diagnostic with GDB. */
_Noreturn void platform_fail(uint32_t reason);
#endif
