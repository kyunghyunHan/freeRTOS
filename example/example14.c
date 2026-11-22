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

/* Chapter 5 — Example 14: Software timer ID and callback parameter */
static void cb(TimerHandle_t timer) {
    uint32_t id=(uint32_t)(uintptr_t)pvTimerGetTimerID(timer);
    char line[75]; (void)snprintf(line,sizeof line,"timer %s id=%lu\r\n",pcTimerGetName(timer),(unsigned long)id);
    ex_log(line);
}
void example_start(void) {
    TimerHandle_t a=xTimerCreate("TimerA",pdMS_TO_TICKS(700),pdTRUE,(void *)(uintptr_t)1,cb);
    TimerHandle_t b=xTimerCreate("TimerB",pdMS_TO_TICKS(1100),pdTRUE,(void *)(uintptr_t)2,cb);
    configASSERT(a && b);
    configASSERT(xTimerStart(a,0)==pdPASS && xTimerStart(b,0)==pdPASS);
}
