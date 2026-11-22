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

/* Chapter 4 — Example 12: Queue sets */
/* Requires configUSE_QUEUE_SETS=1 in FreeRTOSConfig.h. */
static QueueHandle_t qa,qb;
static QueueSetHandle_t set;
static void producer(void *arg) {
    (void)arg;
    uint32_t a=0,b=100;
    for (;;) {
        if (xQueueSendToBack(qa,&a,0)==pdPASS) a++;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (xQueueSendToBack(qb,&b,0)==pdPASS) b++;
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
static void consumer(void *arg) {
    (void)arg;
    for (;;) {
        QueueSetMemberHandle_t selected=xQueueSelectFromSet(set,portMAX_DELAY);
        uint32_t v;
        if (selected != NULL && xQueueReceive((QueueHandle_t)selected,&v,0)==pdPASS) {
            char buf[65]; (void)snprintf(buf,sizeof buf,"Queue %c: %lu\r\n",selected==qa?'A':'B',(unsigned long)v);
            ex_log(buf);
        }
    }
}
void example_start(void) {
    qa=xQueueCreate(3,sizeof(uint32_t)); qb=xQueueCreate(3,sizeof(uint32_t));
    configASSERT(qa && qb);
    set=xQueueCreateSet(6); configASSERT(set);
    configASSERT(xQueueAddToSet(qa,set)==pdPASS && xQueueAddToSet(qb,set)==pdPASS);
    configASSERT(xTaskCreate(producer,"producer",256,NULL,1,NULL)==pdPASS);
    configASSERT(xTaskCreate(consumer,"consumer",256,NULL,2,NULL)==pdPASS);
}
