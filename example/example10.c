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

/* Chapter 4 — Example 10: Blocking when receiving from a queue */
static QueueHandle_t q;
static void sender(void *arg) {
    (void)arg;
    uint32_t value = 0;
    for (;;) {
        value++;
        configASSERT(xQueueSendToBack(q, &value, portMAX_DELAY) == pdPASS);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}
static void receiver(void *arg) {
    (void)arg;
    uint32_t value;
    for (;;) {
        if (xQueueReceive(q, &value, portMAX_DELAY) == pdPASS) {
            char buf[64]; (void)snprintf(buf, sizeof buf, "received %lu\r\n", (unsigned long)value);
            ex_log(buf);
        }
    }
}
void example_start(void) {
    q=xQueueCreate(4, sizeof(uint32_t)); configASSERT(q != NULL);
    configASSERT(xTaskCreate(sender, "sender", 256, NULL, 1, NULL) == pdPASS);
    configASSERT(xTaskCreate(receiver, "receiver", 256, NULL, 2, NULL) == pdPASS);
}
