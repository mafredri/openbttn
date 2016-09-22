#ifndef LED_H
#define LED_H

#include "syscfg.h"

void leds_init(void);
void leds_set_brightness(uint32_t red, uint32_t green, uint32_t blue);
void leds_shift(uint32_t bits);

#endif /* LED_H */
