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

/* Chapter 6 — Example 19: Sending and receiving a queue item from ISR */
static QueueHandle_t q;
static void receiver(void *arg) {
    (void)arg; uint32_t event;
    for (;;) {
        if (xQueueReceive(q,&event,portMAX_DELAY)==pdPASS) {
            char line[55]; (void)snprintf(line,sizeof line,"IRQ event=%lu\r\n",(unsigned long)event);
            ex_log(line);
        }
    }
}
void example_button_irq(void) {
    BaseType_t woken=pdFALSE;
    uint32_t event=13;
    if (q) (void)xQueueSendToBackFromISR(q,&event,&woken);
    portYIELD_FROM_ISR(woken);
}
void example_start(void) {
    q=xQueueCreate(8,sizeof(uint32_t)); configASSERT(q);
    configASSERT(xTaskCreate(receiver,"receiver",256,NULL,3,NULL)==pdPASS);
}
