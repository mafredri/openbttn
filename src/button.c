#include <stdio.h>

#include "button.h"

volatile bool button_pressed;

void button_enable(void) {
  rcc_periph_clock_enable(RCC_GPIOC);
  rcc_periph_clock_enable(RCC_SYSCFG);

  // PC.6 (Button) and PC.13 (Button interrupt)
  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO6 | GPIO13);
  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_400KHZ,
                          GPIO6 | GPIO13);

  exti_select_source(EXTI13, GPIOC);
  button_pressed = false;
  exti_set_trigger(EXTI13, EXTI_TRIGGER_RISING);
  exti_enable_request(EXTI13);

  nvic_enable_irq(NVIC_EXTI15_10_IRQ);
  nvic_set_priority(NVIC_EXTI15_10_IRQ, 4 << 4);
}
