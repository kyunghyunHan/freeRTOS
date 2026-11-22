/* STM32F429ZI NUCLEO-144 reference. CubeMX must initialize USART3
 * (PD8 TX, PD9 RX) and LD1 GPIOB PB0 output.
 * Keep a single UART owner in production; this BSP is for teaching. */
#include "main.h"
#include "example_bsp.h"
#include <string.h>
extern UART_HandleTypeDef huart3;
void ex_log(const char *text) {
    if (text) (void)HAL_UART_Transmit(&huart3, (uint8_t *)text, (uint16_t)strlen(text), 1000);
}
void ex_led_toggle(void) { HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0); }
/* For IRQ examples, put this callback in stm32f4xx_it.c or another user file
 * only if a callback has not already been defined. Keep NVIC priority valid.
 *
 * void HAL_GPIO_EXTI_Callback(uint16_t pin) {
 *     if (pin == GPIO_PIN_13) example_button_irq();
 * }
 */
