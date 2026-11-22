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

/* Chapter 8 — Example 23: Synchronizing tasks with xEventGroupSync */
#define SYNC_A ((EventBits_t)(1u<<0))
#define SYNC_B ((EventBits_t)(1u<<1))
#define SYNC_C ((EventBits_t)(1u<<2))
#define ALL_BITS (SYNC_A|SYNC_B|SYNC_C)
static EventGroupHandle_t barrier;
static void party(void *arg) {
    EventBits_t mine=(EventBits_t)(uintptr_t)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)mine*250u));
        ex_log("Arrived at barrier\r\n");
        EventBits_t result=xEventGroupSync(barrier,mine,ALL_BITS,pdMS_TO_TICKS(5000));
        if ((result & ALL_BITS)==ALL_BITS) ex_log("Barrier passed\r\n");
        else ex_log("Barrier timeout\r\n");
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
void example_start(void) {
    barrier=xEventGroupCreate(); configASSERT(barrier);
    configASSERT(xTaskCreate(party,"A",256,(void *)(uintptr_t)SYNC_A,2,NULL)==pdPASS);
    configASSERT(xTaskCreate(party,"B",256,(void *)(uintptr_t)SYNC_B,2,NULL)==pdPASS);
    configASSERT(xTaskCreate(party,"C",256,(void *)(uintptr_t)SYNC_C,2,NULL)==pdPASS);
}
