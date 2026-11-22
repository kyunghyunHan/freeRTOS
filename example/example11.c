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

/* Chapter 4 — Example 11: Blocking while sending structures to a full queue */
typedef struct { uint32_t id; uint32_t value; } Message;
static QueueHandle_t q;
static void sender(void *arg) {
    (void)arg;
    Message m={1,0};
    for (;;) {
        m.value++;
        if (xQueueSendToBack(q, &m, pdMS_TO_TICKS(700)) != pdPASS)
            ex_log("Queue full -> sender timeout\r\n");
        else
            ex_log("struct sent\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
static void receiver(void *arg) {
    (void)arg;
    Message m;
    for (;;) {
        if (xQueueReceive(q, &m, portMAX_DELAY) == pdPASS) {
            char buf[64]; (void)snprintf(buf,sizeof buf,"recv id=%lu value=%lu\r\n",(unsigned long)m.id,(unsigned long)m.value);
            ex_log(buf);
        }
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}
void example_start(void) {
    q=xQueueCreate(2,sizeof(Message)); configASSERT(q);
    configASSERT(xTaskCreate(sender,"send",256,NULL,2,NULL)==pdPASS);
    configASSERT(xTaskCreate(receiver,"recv",256,NULL,1,NULL)==pdPASS);
}
