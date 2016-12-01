#ifndef SYSCFG_H
#define SYSCFG_H

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <libopencmsis/core_cm3.h>

#define CORE_CLOCK (uint32_t)32000000
#ifndef SOCKD_PORT
#define SOCKD_PORT 8774
#endif

extern volatile uint32_t g_SystemTick;
extern volatile uint32_t g_SystemDelay;

void delay(volatile uint32_t ms);

#endif /* SYSCFG_H */
