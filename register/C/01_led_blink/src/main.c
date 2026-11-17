#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
static void blink_task(void *argument)
{
    (void)argument;
    for (;;) {
        board_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
int main(void)
{
    board_init();
    if (xTaskCreate(blink_task, "blink", 256, NULL, 1, NULL) != pdPASS)
        platform_fail(7);
    vTaskStartScheduler();
    platform_fail(6);
}
