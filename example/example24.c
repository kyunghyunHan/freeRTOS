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

/* Chapter 9 — Example 24: Task notification as binary semaphore */
static TaskHandle_t worker_handle;
static void worker(void *arg) {
    (void)arg;
    for (;;) {
        uint32_t n=ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        if (n>0) {
            char line[70]; (void)snprintf(line,sizeof line,"binary-style notify count=%lu\r\n",(unsigned long)n);
            ex_log(line);
        }
    }
}
void example_button_irq(void) {
    BaseType_t woken=pdFALSE;
    if (worker_handle) vTaskNotifyGiveFromISR(worker_handle,&woken);
    portYIELD_FROM_ISR(woken);
}
void example_start(void) {
    configASSERT(xTaskCreate(worker,"worker",256,NULL,3,&worker_handle)==pdPASS);
}
