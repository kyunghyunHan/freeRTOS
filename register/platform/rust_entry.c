#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
/* Rust exports a C ABI function which never returns or unwinds. */
extern void rust_task(void) __attribute__((noreturn));
static void rust_entry(void *argument)
{
    (void)argument;
    rust_task();
}
int main(void)
{
    board_init();
    if (xTaskCreate(rust_entry, "rust", 512, NULL, 1, NULL) != pdPASS)
        platform_fail(7);
    vTaskStartScheduler();
    platform_fail(6);
}
