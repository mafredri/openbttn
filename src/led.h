#ifndef LED_H
#define LED_H

#include "syscfg.h"

typedef void (*LedToggleHandler)(uint32_t ticks);

typedef struct LedTickState {
  volatile uint16_t speed;
  volatile uint32_t ticks;
  LedToggleHandler toggleFunc;
  volatile bool enabled;
} LedTickState;

void leds_init(void);
void leds_set_brightness(uint32_t red, uint32_t green, uint32_t blue);
void leds_shift(uint32_t bits);

void led_TickConfigure(uint16_t speed, LedToggleHandler handler);
void led_TickEnable(void);
void led_TickDisable(void);

void led_TickHandlerRecovery(uint32_t ticks);

void led_SysTickHandler(uint32_t duration);

#endif /* LED_H */
