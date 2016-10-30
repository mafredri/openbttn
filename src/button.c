#include <stdio.h>

#include "button.h"

ButtonState g_button;

void button_Init(void) {
  rcc_periph_clock_enable(RCC_GPIOC);
  rcc_periph_clock_enable(RCC_SYSCFG);

  // PC.6 (Button) and PC.13 (Button interrupt)
  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO6 | GPIO13);
  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_400KHZ,
                          GPIO6 | GPIO13);

  exti_select_source(EXTI13, GPIOC);
  exti_set_trigger(EXTI13, EXTI_TRIGGER_RISING);
  exti_enable_request(EXTI13);

  nvic_set_priority(NVIC_EXTI15_10_IRQ, (4 << 4));
  nvic_enable_irq(NVIC_EXTI15_10_IRQ);

  button_Reset();
  g_button.pressed = gpio_get(BUTTON_GPIO_PORT, BUTTON_PIN_IT);
}

void button_Reset(void) {
  g_button.pressed = false;
  g_button.duration = 0;
}

bool button_IsPressed(void) { return g_button.pressed; }

uint32_t button_PressedDuration(void) { return g_button.duration; }

void button_SysTickHandler(void) {
  if (g_button.pressed) {
    if (gpio_get(BUTTON_GPIO_PORT, BUTTON_PIN_IT)) {
      g_button.duration++;
    } else {
      g_button.pressed = false;
    }
  }
}

void BUTTON_ISR(void) {
  if (exti_get_flag_status(EXTI13)) {
    if (gpio_get(BUTTON_GPIO_PORT, BUTTON_PIN_IT)) {
      g_button.pressed = true;
      g_button.duration = 0;
    }
    exti_reset_request(EXTI13);
  }
}
