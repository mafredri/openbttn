#include <stdio.h>

#include "led.h"

LedTickState g_LedTick;

static void ledPowerGpioConfigure(uint32_t rcc, uint32_t port, uint16_t pin);
static void powerGpioSetup(void);
static void powerPwmSetup(void);
static void hc595Init(void);
static void hc595Send(uint32_t bits);

void led_Init(void) {
  LedTickState *ledTick = &g_LedTick;

  ledTick->enabled = false;
  ledTick->ticks = 0;
  ledTick->speed = 0;

  hc595Init();      // Init the shift register
  led_Set(0);       // Reset leds (off)
  powerGpioSetup(); // Init GPIOs that control led power.
  powerPwmSetup();  // Timer manages led power output through PWM.
}

void led_SetBrightness(uint32_t red, uint32_t green, uint32_t blue) {
  timer_set_oc_value(LED_TIMER, TIM_OC1, red);
  timer_set_oc_value(LED_TIMER, TIM_OC2, green);
  timer_set_oc_value(LED_TIMER, TIM_OC3, blue);
}

void led_Set(uint32_t bits) { hc595Send(bits); }

static void powerGpioSetup(void) {
  ledPowerGpioConfigure(RCC_LED_RED, LED_RED_PORT, LED_RED_PIN);
  ledPowerGpioConfigure(RCC_LED_GREEN, LED_GREEN_PORT, LED_GREEN_PIN);
  ledPowerGpioConfigure(RCC_LED_BLUE, LED_BLUE_PORT, LED_BLUE_PIN);
}

static void ledPowerGpioConfigure(uint32_t rcc, uint32_t port, uint16_t pin) {
  rcc_periph_clock_enable(rcc);
  gpio_clear(port, pin);

  gpio_mode_setup(port, GPIO_MODE_AF, GPIO_PUPD_NONE, pin);
  gpio_set_output_options(port, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, pin);
  gpio_set_af(port, LED_AF, pin);
}

// Initialise the led timer for controlling the power output through PWM.
static void powerPwmSetup(void) {
  rcc_periph_clock_enable(RCC_LED_TIMER);

  timer_reset(LED_TIMER);
  timer_set_mode(LED_TIMER, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE,
                 TIM_CR1_DIR_UP);
  // Prescaler: 32MHz / 8MHz - 1 = 3
  timer_set_prescaler(LED_TIMER, (uint32_t)(CORE_CLOCK / 8000000 - 1));
  // Period: 32MHz / 1000Hz / 3 (prescaler) / 12 - 1 = ~887
  timer_set_period(LED_TIMER, (uint32_t)887);

  timer_set_oc_mode(LED_TIMER, TIM_OC1, TIM_OCM_PWM1);
  timer_enable_oc_output(LED_TIMER, TIM_OC1);
  timer_set_oc_polarity_high(LED_TIMER, TIM_OC1);
  timer_set_oc_value(LED_TIMER, TIM_OC1, 0);
  timer_enable_oc_preload(LED_TIMER, TIM_OC1);

  timer_set_oc_mode(LED_TIMER, TIM_OC2, TIM_OCM_PWM1);
  timer_enable_oc_output(LED_TIMER, TIM_OC2);
  timer_set_oc_polarity_high(LED_TIMER, TIM_OC2);
  timer_set_oc_value(LED_TIMER, TIM_OC2, 0);
  timer_enable_oc_preload(LED_TIMER, TIM_OC2);

  timer_set_oc_mode(LED_TIMER, TIM_OC3, TIM_OCM_PWM1);
  timer_enable_oc_output(LED_TIMER, TIM_OC3);
  timer_set_oc_polarity_high(LED_TIMER, TIM_OC3);
  timer_set_oc_value(LED_TIMER, TIM_OC3, 0);
  timer_enable_oc_preload(LED_TIMER, TIM_OC3);

  timer_enable_counter(LED_TIMER);
}

void led_TickConfigure(uint16_t speed, LedToggleHandler toggleFunc) {
  LedTickState *ledTick = &g_LedTick;
  ledTick->speed = speed;
  ledTick->toggleFunc = toggleFunc;
}

void led_TickEnable(void) {
  LedTickState *ledTick = &g_LedTick;
  ledTick->enabled = true;
}

void led_TickHandlerRecovery(uint32_t ticks) {
  if (ticks >= 10) {
    return;
  }

  switch ((ticks % 10)) {
  case 0:
    led_Set(0x01 << 16);
    break;
  case 1:
    led_Set(0x03 << 16);
    break;
  case 2:
    led_Set(0x07 << 16);
    break;
  case 3:
    led_Set(0x0F << 16);
    break;
  case 4:
    led_Set(0x1F << 16);
    break;
  case 5:
  case 7:
  case 9:
    led_Set(0x3F << 16);
    break;
  case 6:
  case 8:
    led_Set(0);
    break;
  }
}

void led_TickDisable(void) {
  LedTickState *ledTick = &g_LedTick;
  ledTick->enabled = false;
}

void led_SysTickHandler(uint32_t duration) {
  LedTickState *ledTick = &g_LedTick;

  if (ledTick->enabled && ((duration % ledTick->speed) == 0)) {
    (*ledTick->toggleFunc)(ledTick->ticks++);
  }
}

// hc595Init initialises the HC595 shift registers, the bttn has three of them
// chained together.
static void hc595Init(void) {
  rcc_periph_clock_enable(RCC_HC595);

  gpio_clear(HC595_PORT, HC595_GPIOS);

  gpio_mode_setup(HC595_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HC595_GPIOS);
  gpio_set_output_options(HC595_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                          HC595_GPIOS);
}

// hc595Send sends up to twenty-four bits (eight per register) used to control
// the bttn leds. When a bit is set, the corresponding led lights up.
static void hc595Send(uint32_t bits) {
  uint8_t i;

  // Three shift registers of eight bits each adds up to 24.
  for (i = 0; i < 24; i++) {
    // If the bit is high, we clock low to turn led on.
    if (bits & 0x800000) {
      gpio_clear(HC595_PORT, HC595_DS);
    } else {
      gpio_set(HC595_PORT, HC595_DS);
    }

    // Clock the current bit into the shift register.
    gpio_set(HC595_PORT, HC595_SHCP);
    gpio_clear(HC595_PORT, HC595_SHCP);

    bits <<= 1;
  }

  // Clock the current bits into the storage register.
  gpio_set(HC595_PORT, HC595_STCP);
  gpio_clear(HC595_PORT, HC595_STCP);

  gpio_clear(HC595_PORT, HC595_DS);
}
