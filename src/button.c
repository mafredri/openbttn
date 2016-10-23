#include <stdio.h>

#include "button.h"

volatile bool g_ButtonPressed = false;

void button_enable(void) {
  rcc_periph_clock_enable(RCC_GPIOC);
  rcc_periph_clock_enable(RCC_SYSCFG);

  // PC.6 (Button) and PC.13 (Button interrupt)
  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO6 | GPIO13);
  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_400KHZ,
                          GPIO6 | GPIO13);

  exti_select_source(EXTI13, GPIOC);
  g_ButtonPressed = false;
  exti_set_trigger(EXTI13, EXTI_TRIGGER_RISING);
  exti_enable_request(EXTI13);

  nvic_enable_irq(NVIC_EXTI15_10_IRQ);
  nvic_set_priority(NVIC_EXTI15_10_IRQ, 4 << 4);
}

// TODO: Not system tick.
uint32_t button_PressedDuration(void) { return g_SystemTick; }

void BUTTON_ISR(void) {
  if (exti_get_flag_status(EXTI13)) {
    exti_reset_request(EXTI13);
    g_ButtonPressed = true;
  }
}
