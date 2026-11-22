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

/* Chapter 3 — Example 07: Defining an Idle Task hook */
/* Requires configUSE_IDLE_HOOK=1 in FreeRTOSConfig.h.
 * Never block, call vTaskDelay(), or wait for a peripheral inside this hook. */
static volatile uint32_t idle_counter;
void vApplicationIdleHook(void) { idle_counter++; }
static void reporter(void *arg) {
    (void)arg;
    for (;;) {
        char line[72];
        (void)snprintf(line, sizeof line, "Idle visits=%lu\r\n", (unsigned long)idle_counter);
        ex_log(line);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void example_start(void) {
    configASSERT(xTaskCreate(reporter, "report", 256, NULL, 1, NULL) == pdPASS);
}
