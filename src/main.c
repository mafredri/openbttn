#include <stdio.h>

#include "button.h"
#include "debug.h"
#include "led.h"
#include "syscfg.h"

volatile uint32_t system_delay = 0;

const struct rcc_clock_scale bttn_clock_config;
static void clock_setup(void);
static void gpio_setup(void);

int main(void) {
  clock_setup();

  gpio_setup();

  // Power on LED8.
  gpio_set(GPIOA, GPIO5);

  // Enable board power (power switch).
  gpio_set(GPIOB, GPIO5);
  delay(80);
  // Allow power to board (power control).
  gpio_set(GPIOC, GPIO7);
  delay(80);

  debug_init();

  // Power on Wifi module.
  gpio_set(GPIOB, GPIO2);

  button_enable();

  leds_init();
  leds_set_brightness(119, 119, 119);

  printf("\nBootup complete!\n");

  uint32_t led_bits = 0b100100100100100100100100;

  printf("Entering main loop!\n");

  while (1) {
    leds_shift(led_bits);
    led_bits = (led_bits << 1) + ((led_bits & 0x800000) >> 23);
    delay(1000);
  }

  return 0;
}

const struct rcc_clock_scale bttn_clock_config = {
    .pll_source = RCC_CFGR_PLLSRC_HSE_CLK,  // Use HSE
    .pll_mul = RCC_CFGR_PLLMUL_MUL8,
    .pll_div = RCC_CFGR_PLLDIV_DIV2,
    .hpre = RCC_CFGR_HPRE_SYSCLK_NODIV,
    .ppre1 = RCC_CFGR_PPRE1_HCLK_NODIV,
    .ppre2 = RCC_CFGR_PPRE2_HCLK_NODIV,
    .voltage_scale = PWR_SCALE1,
    .flash_config = FLASH_ACR_LATENCY_1WS,
    .ahb_frequency = CORE_CLOCK,
    .apb1_frequency = CORE_CLOCK,
    .apb2_frequency = CORE_CLOCK,
};

static void clock_setup(void) {
  rcc_clock_setup_pll(&bttn_clock_config);

  systick_set_reload((uint32_t)(CORE_CLOCK / 1000));  // 1ms
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_interrupt_enable();
  systick_counter_enable();
}

static void gpio_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);

  gpio_clear(GPIOB, GPIO5);  // Boot1 pin
  gpio_clear(GPIOB, GPIO2);  // Power switch
  gpio_clear(GPIOC, GPIO7);  // Power control
  gpio_clear(GPIOA, GPIO5);  // LED8

  // Boot pin
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);  // PB.5
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO5);

  // Power switch
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);  // PB.2
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO2);

  // Power control
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);  // PC.7
  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO7);

  // LED8 (power indicator)
  gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);  // PA.5
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO5);
}

void delay(volatile uint32_t ms) {
  system_delay = ms;
  while (system_delay != 0)
    ;
}
