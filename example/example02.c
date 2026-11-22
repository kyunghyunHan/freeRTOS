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

/* Chapter 3 — Example 02: Using task parameters */
static const char msg_a[] = "Hello from Task A\r\n";
static const char msg_b[] = "Hello from Task B\r\n";
static void parameter_task(void *argument) {
    const char *message = (const char *)argument;
    for (;;) { ex_log(message); vTaskDelay(pdMS_TO_TICKS(500)); }
}
void example_start(void) {
    configASSERT(xTaskCreate(parameter_task, "A", 256, (void *)msg_a, 1, NULL) == pdPASS);
    configASSERT(xTaskCreate(parameter_task, "B", 256, (void *)msg_b, 1, NULL) == pdPASS);
}
