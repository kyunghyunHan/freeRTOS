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

/* Chapter 8 — Example 22: Event groups: AND and OR waiting */
#define BIT_A ((EventBits_t)(1u<<0))
#define BIT_B ((EventBits_t)(1u<<1))
#define BIT_C ((EventBits_t)(1u<<2))
static EventGroupHandle_t group;
static void setter(void *arg) {
    (void)arg;
    for (;;) {
        (void)xEventGroupClearBits(group,BIT_A|BIT_B|BIT_C);
        vTaskDelay(pdMS_TO_TICKS(300)); (void)xEventGroupSetBits(group,BIT_A);
        vTaskDelay(pdMS_TO_TICKS(300)); (void)xEventGroupSetBits(group,BIT_B);
        vTaskDelay(pdMS_TO_TICKS(300)); (void)xEventGroupSetBits(group,BIT_C);
        vTaskDelay(pdMS_TO_TICKS(1300));
    }
}
static void waiter_and(void *arg) {
    (void)arg;
    for (;;) {
        EventBits_t bits=xEventGroupWaitBits(group,BIT_A|BIT_B|BIT_C,pdFALSE,pdTRUE,portMAX_DELAY);
        if ((bits & (BIT_A|BIT_B|BIT_C))==(BIT_A|BIT_B|BIT_C)) ex_log("AND condition met\r\n");
        vTaskDelay(pdMS_TO_TICKS(1600)); /* Prevent repeated immediate matches. */
    }
}
static void waiter_or(void *arg) {
    (void)arg;
    for (;;) {
        EventBits_t bits=xEventGroupWaitBits(group,BIT_A|BIT_B|BIT_C,pdFALSE,pdFALSE,portMAX_DELAY);
        if ((bits & (BIT_A|BIT_B|BIT_C))!=0u) ex_log("OR condition met\r\n");
        vTaskDelay(pdMS_TO_TICKS(1600));
    }
}
void example_start(void) {
    group=xEventGroupCreate(); configASSERT(group);
    configASSERT(xTaskCreate(setter,"setter",256,NULL,1,NULL)==pdPASS);
    configASSERT(xTaskCreate(waiter_and,"and",256,NULL,2,NULL)==pdPASS);
    configASSERT(xTaskCreate(waiter_or,"or",256,NULL,2,NULL)==pdPASS);
}
