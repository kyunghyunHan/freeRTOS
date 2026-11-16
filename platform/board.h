#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stddef.h>
void board_init(void);
void board_led_toggle(void);
/* B1 USER on PC13: returns 1 while pressed, 0 while released. */
uint32_t board_button_pressed(void);
void board_uart_write(const uint8_t *data, size_t len);
/* Stops with interrupts disabled. Inspect g_diagnostic with GDB. */
_Noreturn void platform_fail(uint32_t reason);
#endif
