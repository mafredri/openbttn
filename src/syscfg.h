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

#define RCC_DEBUG_USART RCC_USART1
#define DEBUG_GPIO_PORT GPIOA
#define DEBUG_USART USART1
#define DEBUG_PIN_TX GPIO9

#define LED_TIMER TIM3
#define RCC_LED_TIMER RCC_TIM3

#define HC595_STCP GPIO9   // PC.9
#define HC595_SHCP GPIO10  // PC.10
#define HC595_DS GPIO11    // PC.11
#define HC595_GPIOS HC595_STCP | HC595_SHCP | HC595_DS

extern volatile uint32_t g_SystemTick;
extern volatile uint32_t g_SystemDelay;

void delay(volatile uint32_t ms);

#endif /* SYSCFG_H */
