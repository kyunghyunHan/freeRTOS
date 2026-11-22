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

/* Chapter 6 — Example 18: Deferring interrupt work to the daemon task */
/* Requires configUSE_TIMERS=1 and a live timer daemon task. */
static void deferred(void *arg, uint32_t value) {
    (void)arg; (void)value;
    ex_log("Deferred processing in Timer Daemon Task\r\n");
}
void example_button_irq(void) {
    BaseType_t woken=pdFALSE;
    (void)xTimerPendFunctionCallFromISR(deferred,NULL,0,&woken);
    portYIELD_FROM_ISR(woken);
}
void example_start(void) { /* No worker task is required. */ }
