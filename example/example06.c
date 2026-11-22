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

/* Chapter 3 — Example 06: Combining blocking and non-blocking tasks */
static void periodic(void *arg) {
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) { ex_log("Periodic task\r\n"); vTaskDelayUntil(&last, pdMS_TO_TICKS(300)); }
}
static volatile uint32_t work_count;
static void continuous(void *arg) {
    (void)arg;
    for (;;) {
        work_count++; /* Never blocks; lower priority work will be preempted. */
        if ((work_count & 0xfffffu) == 0u) { ex_led_toggle(); }
    }
}
void example_start(void) {
    configASSERT(xTaskCreate(continuous, "continuous", 256, NULL, 1, NULL) == pdPASS);
    configASSERT(xTaskCreate(periodic, "periodic", 256, NULL, 2, NULL) == pdPASS);
}
