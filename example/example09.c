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

/* Chapter 3 — Example 09: Deleting tasks */
static TaskHandle_t temporary_handle;
static void temporary(void *arg) {
    (void)arg;
    for (;;) { ex_log("Temporary task\r\n"); vTaskDelay(pdMS_TO_TICKS(200)); }
}
static void killer(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1300));
    ex_log("Deleting temporary task\r\n");
    vTaskDelete(temporary_handle);
    temporary_handle = NULL;
    vTaskDelete(NULL); /* Own TCB/stack cleanup is done later by Idle Task. */
}
void example_start(void) {
    configASSERT(xTaskCreate(temporary, "temporary", 256, NULL, 1, &temporary_handle) == pdPASS);
    configASSERT(xTaskCreate(killer, "killer", 256, NULL, 2, NULL) == pdPASS);
}
