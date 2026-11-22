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

/* Chapter 6 — Example 16: Binary semaphore to synchronize ISR with task */
/* B1/PC13 EXTI falling/rising edge must be configured in CubeMX.
 * Call example_button_irq() from HAL_GPIO_EXTI_Callback(GPIO_PIN_13).
 * Do not define the callback twice. */
static SemaphoreHandle_t sem;
static void worker(void *arg) {
    (void)arg;
    for (;;) {
        if (xSemaphoreTake(sem,portMAX_DELAY)==pdTRUE) {
            ex_led_toggle(); ex_log("Button IRQ -> binary semaphore -> worker\r\n");
        }
    }
}
void example_button_irq(void) {
    BaseType_t woken=pdFALSE;
    if (sem) (void)xSemaphoreGiveFromISR(sem,&woken);
    portYIELD_FROM_ISR(woken);
}
void example_start(void) {
    sem=xSemaphoreCreateBinary(); configASSERT(sem);
    configASSERT(xTaskCreate(worker,"worker",256,NULL,3,NULL)==pdPASS);
}
