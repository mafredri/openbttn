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

#define BUTTON_GPIO_PORT GPIOC
#define BUTTON_PIN GPIO6
#define BUTTON_PIN_IT GPIO13
#define BUTTON_EXTI EXTI13
#define BUTTON_isr exti15_10_isr
#define BUTTON_NVIC NVIC_EXTI15_10_IRQ

#define URL_LENGTH 100

#define RCC_WIFI_USART RCC_USART3
#define WIFI_GPIO_PORT GPIOB
#define WIFI_USART USART3
#define WIFI_GPIO_TX GPIO10
#define WIFI_GPIO_RX GPIO11
#define WIFI_GPIO_CTS GPIO13
#define WIFI_GPIO_RTS GPIO14
#define WIFI_ISR usart3_isr
#define WIFI_NVIC_IRQ NVIC_USART3_IRQ

#define LED_TIMER TIM3
#define RCC_LED_TIMER RCC_TIM3

#define HC595_STCP GPIO9   // PC.9
#define HC595_SHCP GPIO10  // PC.10
#define HC595_DS GPIO11    // PC.11
#define HC595_GPIOS HC595_STCP | HC595_SHCP | HC595_DS

extern volatile uint32_t system_delay;
extern volatile bool button_pressed;

void delay(volatile uint32_t ms);

#endif /* SYSCFG_H */
