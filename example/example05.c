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

/* Chapter 3 — Example 05: Using vTaskDelayUntil (periodic task) */
static void periodic(void *arg) {
    (void)arg;
    TickType_t previous = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(500);
    for (;;) { ex_led_toggle(); ex_log("periodic tick\r\n"); vTaskDelayUntil(&previous, period); }
}
void example_start(void) {
    configASSERT(xTaskCreate(periodic, "periodic", 256, NULL, 2, NULL) == pdPASS);
}
