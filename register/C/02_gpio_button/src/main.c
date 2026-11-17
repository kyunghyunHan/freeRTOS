#include "FreeRTOS.h"
#include "task.h"
#include "board.h"

enum { POLL_MS = 10, DEBOUNCE_MS = 30 };

static void button_task(void *argument)
{
    (void)argument;
    uint32_t stable_pressed = 0;
    uint32_t candidate_pressed = 0;
    uint32_t unchanged_ms = 0;

    for (;;) {
        uint32_t pressed = board_button_pressed();
        if (pressed != candidate_pressed) {
            candidate_pressed = pressed;
            unchanged_ms = 0;
        } else if (unchanged_ms < DEBOUNCE_MS) {
            unchanged_ms += POLL_MS;
        }

        /* Debounce both edges; only a new stable press toggles the LED. */
        if (unchanged_ms >= DEBOUNCE_MS && stable_pressed != candidate_pressed) {
            stable_pressed = candidate_pressed;
            if (stable_pressed)
                board_led_toggle();
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

int main(void)
{
    board_init();
    if (xTaskCreate(button_task, "button", 256, NULL, 1, NULL) != pdPASS)
        platform_fail(7);
    vTaskStartScheduler();
    platform_fail(6);
}
