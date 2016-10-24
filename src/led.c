#include <stdio.h>

#include "led.h"

LedTickState g_LedTick;

static void led_power_init(void);
static void led_timer_init(void);
static void hc595_init(void);
static void hc595_send(uint32_t bits);

void leds_init(void) {
  LedTickState *ledTick = &g_LedTick;

  ledTick->enabled = false;
  ledTick->ticks = 0;
  ledTick->speed = 0;

  hc595_init();     // Init the shift register
  leds_shift(0);    // Reset leds (off)
  led_power_init(); // Init GPIOs that control led power.
  led_timer_init(); // Timer manages led power output through PWM.
}

void leds_set_brightness(uint32_t red, uint32_t green, uint32_t blue) {
  timer_set_oc_value(LED_TIMER, TIM_OC1, red);
  timer_set_oc_value(LED_TIMER, TIM_OC2, green);
  timer_set_oc_value(LED_TIMER, TIM_OC3, blue);
}

void leds_shift(uint32_t bits) { hc595_send(bits); }

static void led_power_init(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);

  gpio_clear(GPIOA, GPIO6 | GPIO7);
  gpio_clear(GPIOB, GPIO0); // LED (Blue)

  // PA.6 (Red, TIM3_CH1) & PA.7 (Green, TIM3_CH2)
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO7);
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                          GPIO6 | GPIO7);
  gpio_set_af(GPIOA, GPIO_AF2, GPIO6 | GPIO7);

  // PB.0 (Blue, TIM3_CH3)
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO0);
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                          GPIO6 | GPIO0);
  gpio_set_af(GPIOB, GPIO_AF2, GPIO6 | GPIO0);
}

// Initialise the led timer for controlling the power output through PWM.
static void led_timer_init(void) {
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
    leds_shift(0x01 << 16);
    break;
  case 1:
    leds_shift(0x03 << 16);
    break;
  case 2:
    leds_shift(0x07 << 16);
    break;
  case 3:
    leds_shift(0x0F << 16);
    break;
  case 4:
    leds_shift(0x1F << 16);
    break;
  case 5:
  case 7:
  case 9:
    leds_shift(0x3F << 16);
    break;
  case 6:
  case 8:
    leds_shift(0);
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

// Shift register initialisation for HC595.
static void hc595_init(void) {
  rcc_periph_clock_enable(RCC_GPIOC);

  gpio_clear(GPIOC, HC595_GPIOS);

  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HC595_GPIOS);
  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, HC595_GPIOS);
}

static void hc595_send(uint32_t bits) {
  // There's only 3 shift registers
  // daisy-chained, so 24 bits is
  // the limit.
  uint8_t i;
  for (i = 0; i < 24; i++) {
    // If the bit is high, we clock
    // low to turn led on.
    if (bits & 0x800000) {
      gpio_clear(GPIOC, HC595_DS);
    } else {
      gpio_set(GPIOC, HC595_DS);
    }

    // Clock the current bit into the
    // shift register.
    gpio_set(GPIOC, HC595_SHCP);
    gpio_clear(GPIOC, HC595_SHCP);

    bits <<= 1;
  }

  // Clock the current bits into
  // the storage register.
  gpio_set(GPIOC, HC595_STCP);
  gpio_clear(GPIOC, HC595_STCP);

  gpio_clear(GPIOC, HC595_DS);
}
