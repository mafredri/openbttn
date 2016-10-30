#ifndef LED_H
#define LED_H

#include "syscfg.h"

#define LED_TIMER TIM3
#define RCC_LED_TIMER RCC_TIM3
#define LED_AF GPIO_AF2

// PA.6 (Red, TIM3_CH1)
#define RCC_LED_RED RCC_GPIOA
#define LED_RED_PORT GPIOA
#define LED_RED_PIN GPIO6

// PA.7 (Green, TIM3_CH2)
#define RCC_LED_GREEN RCC_GPIOA
#define LED_GREEN_PORT GPIOA
#define LED_GREEN_PIN GPIO7

// PB.0 (Blue, TIM3_CH3)
#define RCC_LED_BLUE RCC_GPIOB
#define LED_BLUE_PORT GPIOB
#define LED_BLUE_PIN GPIO0

#define RCC_HC595 RCC_GPIOC
#define HC595_PORT GPIOC
#define HC595_STCP GPIO9  // PC.9
#define HC595_SHCP GPIO10 // PC.10
#define HC595_DS GPIO11   // PC.11
#define HC595_GPIOS HC595_STCP | HC595_SHCP | HC595_DS

typedef void (*LedToggleHandler)(uint32_t ticks);

typedef struct LedTickState LedTickState;
struct LedTickState {
  volatile uint16_t speed;
  volatile uint32_t ticks;
  uint32_t startTick;
  LedToggleHandler toggleFunc;
  volatile bool enabled;
  volatile bool init;
};

void led_Init(void);
void led_SetBrightness(uint32_t red, uint32_t green, uint32_t blue);
void led_Set(uint32_t bits);

void led_TickConfigure(uint16_t speed, uint32_t startTick,
                       LedToggleHandler handler);
void led_TickEnable(void);
void led_TickDisable(void);

void led_TickHandlerRecoveryInit(uint32_t ticks);
void led_TickHandlerRecovery(uint32_t ticks);
void led_TickHandlerRecoveryLoading(uint32_t ticks);
void led_TickHandlerError(uint32_t ticks);
void led_TickHandlerPending(uint32_t ticks);
void led_TickHandlerGreenCircleFill(uint32_t ticks);
void led_TickHandlerGreenLoading(uint32_t ticks);
void led_TickHandlerGreenSuccess(uint32_t ticks);
void led_TickHandlerBoot(uint32_t ticks);

void led_SysTickHandler(uint32_t duration);

#endif /* LED_H */
