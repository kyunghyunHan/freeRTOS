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

/* Chapter 5 — Example 13: One-shot and auto-reload software timers */
static void callback(TimerHandle_t timer) {
    if (pcTimerGetName(timer)[0]=='O') ex_log("One-shot fired\r\n");
    else ex_log("Auto-reload fired\r\n");
}
void example_start(void) {
    TimerHandle_t once=xTimerCreate("Once",pdMS_TO_TICKS(1500),pdFALSE,NULL,callback);
    TimerHandle_t repeat=xTimerCreate("Repeat",pdMS_TO_TICKS(500),pdTRUE,NULL,callback);
    configASSERT(once && repeat);
    configASSERT(xTimerStart(once,0)==pdPASS);
    configASSERT(xTimerStart(repeat,0)==pdPASS);
}
