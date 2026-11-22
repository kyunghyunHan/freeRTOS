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

/* Chapter 5 — Example 15: Resetting a software timer */
static TimerHandle_t idle_timer;
static void timeout_cb(TimerHandle_t t) { (void)t; ex_log("No activity for 2 seconds\r\n"); }
static void activity(void *arg) {
    (void)arg;
    for (unsigned i=0;i<5;i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
        configASSERT(xTimerReset(idle_timer,portMAX_DELAY)==pdPASS);
        ex_log("Activity -> timer reset\r\n");
    }
    vTaskDelete(NULL); /* timer will finally expire 2 s after final reset */
}
void example_start(void) {
    idle_timer=xTimerCreate("idle",pdMS_TO_TICKS(2000),pdFALSE,NULL,timeout_cb);
    configASSERT(idle_timer);
    configASSERT(xTimerStart(idle_timer,0)==pdPASS);
    configASSERT(xTaskCreate(activity,"activity",256,NULL,2,NULL)==pdPASS);
}
