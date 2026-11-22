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

/* Chapter 3 — Example 08: Changing task priorities */
static TaskHandle_t worker_handle;
static void worker(void *arg) {
    (void)arg;
    for (;;) { ex_log("Worker running\r\n"); vTaskDelay(pdMS_TO_TICKS(300)); }
}
static void manager(void *arg) {
    (void)arg;
    for (;;) {
        vTaskPrioritySet(worker_handle, 3);
        ex_log("Worker priority -> 3\r\n");
        vTaskDelay(pdMS_TO_TICKS(1200));
        vTaskPrioritySet(worker_handle, 1);
        ex_log("Worker priority -> 1\r\n");
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}
void example_start(void) {
    configASSERT(xTaskCreate(worker, "worker", 256, NULL, 1, &worker_handle) == pdPASS);
    configASSERT(xTaskCreate(manager, "manager", 256, NULL, 2, NULL) == pdPASS);
}
