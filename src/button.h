#ifndef BUTTON_H
#define BUTTON_H

#include "syscfg.h"

#define BUTTON_GPIO_PORT GPIOC
#define BUTTON_PIN GPIO6
#define BUTTON_PIN_IT GPIO13
#define BUTTON_EXTI EXTI13
#define BUTTON_ISR exti15_10_isr
#define BUTTON_NVIC NVIC_EXTI15_10_IRQ

extern volatile bool g_ButtonPressed;

void button_enable(void);
uint32_t button_PressedDuration(void);

#endif /* BUTTON_H */
