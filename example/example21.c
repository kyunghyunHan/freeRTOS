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

/* Chapter 7 — Example 21: Gatekeeper task for UART output */
static QueueHandle_t output;
static void gatekeeper(void *arg) {
    (void)arg;
    const char *s;
    for (;;) {
        if (xQueueReceive(output,&s,portMAX_DELAY)==pdPASS) ex_log(s);
    }
}
static void client(void *arg) {
    const char *s=(const char *)arg;
    for (;;) {
        configASSERT(xQueueSendToBack(output,&s,portMAX_DELAY)==pdPASS);
        vTaskDelay(pdMS_TO_TICKS(350));
    }
}
void example_start(void) {
    output=xQueueCreate(10,sizeof(const char *)); configASSERT(output);
    configASSERT(xTaskCreate(gatekeeper,"gatekeeper",256,NULL,3,NULL)==pdPASS);
    configASSERT(xTaskCreate(client,"A",256,(void *)"Gatekeeper: A\r\n",1,NULL)==pdPASS);
    configASSERT(xTaskCreate(client,"B",256,(void *)"Gatekeeper: B\r\n",1,NULL)==pdPASS);
}
