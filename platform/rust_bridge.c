#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "rust_bridge.h"
void rtos_delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks ? ticks : 1);
}
void rtos_led_toggle(void) { board_led_toggle(); }
void rtos_uart_write(const uint8_t *data, size_t len) { board_uart_write(data, len); }
_Noreturn void rtos_panic(void) { platform_fail(5); }
