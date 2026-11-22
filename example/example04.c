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

/* Chapter 3 — Example 04: Blocked state using vTaskDelay */
static void delayed(void *arg) {
    (void)arg;
    for (;;) { ex_led_toggle(); ex_log("Task → Blocked 500ms\r\n"); vTaskDelay(pdMS_TO_TICKS(500)); }
}
void example_start(void) {
    configASSERT(xTaskCreate(delayed, "delay", 256, NULL, 2, NULL) == pdPASS);
}
