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

/* Chapter 3 — Example 01: Creating tasks */
static void task_one(void *arg) {
    (void)arg;
    for (;;) { ex_log("Example 01 / Task 1\r\n"); vTaskDelay(pdMS_TO_TICKS(400)); }
}
static void task_two(void *arg) {
    (void)arg;
    for (;;) { ex_log("Example 01 / Task 2\r\n"); vTaskDelay(pdMS_TO_TICKS(700)); }
}
void example_start(void) {
    configASSERT(xTaskCreate(task_one, "task1", 256, NULL, 1, NULL) == pdPASS);
    configASSERT(xTaskCreate(task_two, "task2", 256, NULL, 1, NULL) == pdPASS);
}
