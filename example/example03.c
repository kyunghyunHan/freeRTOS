/* STM32F429ZI / FreeRTOS native API learning example.
 * This file is a standalone EXAMPLE MODULE, not a complete CubeMX project.
 * Build EXACTLY ONE exampleXX.c per application.
 * BSP: example_bsp.c; call example_start() before vTaskStartScheduler().
 * English API names match the 2016 tutorial; examples are new adaptations.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
#include "example_bsp.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Chapter 3 — Example 03: Experimenting with priorities */
static void high(void *arg) {
    (void)arg;
    for (;;) { ex_log("HIGH priority\r\n"); vTaskDelay(pdMS_TO_TICKS(300)); }
}
static void low(void *arg) {
    (void)arg;
    for (;;) { ex_log("LOW priority\r\n"); vTaskDelay(pdMS_TO_TICKS(300)); }
}
void example_start(void) {
    configASSERT(xTaskCreate(low, "low", 256, NULL, 1, NULL) == pdPASS);
    configASSERT(xTaskCreate(high, "high", 256, NULL, 2, NULL) == pdPASS);
}
