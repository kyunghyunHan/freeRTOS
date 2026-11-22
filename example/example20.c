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

/* Chapter 7 — Example 20: Mutex protects a shared output resource */
static SemaphoreHandle_t mutex;
static void writer(void *arg) {
    const char *name=(const char *)arg;
    for (;;) {
        if (xSemaphoreTake(mutex,portMAX_DELAY)==pdTRUE) {
            ex_log(name); /* Lock covers the complete UART message. */
            configASSERT(xSemaphoreGive(mutex)==pdTRUE);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
void example_start(void) {
    mutex=xSemaphoreCreateMutex(); configASSERT(mutex);
    configASSERT(xTaskCreate(writer,"writerA",256,(void *)"Mutex: AAAAA\r\n",1,NULL)==pdPASS);
    configASSERT(xTaskCreate(writer,"writerB",256,(void *)"Mutex: BBBBB\r\n",1,NULL)==pdPASS);
}
