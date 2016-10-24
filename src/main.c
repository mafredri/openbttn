#include <stdio.h>
#include <string.h>

#include "button.h"
#include "conf.h"
#include "data.h"
#include "debug.h"
#include "led.h"
#include "syscfg.h"
#include "wifi.h"

volatile uint32_t g_SystemTick = 0;
volatile uint32_t g_SystemDelay = 0;

static void clockSetup(void);
static void gpioSetup(void);

int main(void) {
  clockSetup();

  gpioSetup();

  // Power on LED8.
  gpio_set(GPIOA, GPIO5);

  // Enable board power (power switch).
  gpio_set(GPIOB, GPIO5);
  delay(50);
  // Allow power to board (power control).
  gpio_set(GPIOC, GPIO7);
  delay(50);

  debug_Init();
  wifi_Init();

  button_Init();

  led_Init();
  led_SetBrightness(119, 119, 119);

  printf("\nBootup complete!\n");

  conf_Init();

  if (g_buttonPressed || gpio_get(BUTTON_GPIO_PORT, BUTTON_PIN_IT)) {
    led_TickConfigure(500, &led_TickHandlerRecovery);
    led_TickEnable();

    conf_SetTempPassword("openbttn1337");
    wifi_EnableFirstConfig("openbttn", "openbttn1337");
    wifi_WaitState(WIFI_STATE_UP);
    wifi_CreateFileInRam("firstset.html", "text/html",
                         (char *)&g_DataFirstsetHtml[0],
                         DATA_FIRSTSET_HTML_LENGTH);

    // TODO: Escape!
    while (1) {
      conf_HandleChange();
      wifi_HandleChange();
    }

    led_TickDisable();
  } else {
    // Show blue light until WIFI is ready.
    led_Set(0xff0000);
  }

  wifi_CreateFileInRam("index.html", "text/html", (char *)&g_DataIndexHtml[0],
                       DATA_INDEX_HTML_LENGTH);
  conf_CreateConfigJson();

  printf("Waiting for WIFI UP...\n");
  wifi_WaitState(WIFI_STATE_UP);

  // WIFI is up, power off leds.
  led_Set(0);

  printf("Entering main loop!\n");

  while (1) {
    conf_HandleChange();
    wifi_HandleChange();

    if (g_buttonPressed) {
      uint16_t httpStatus;

      httpStatus = wifi_HttpGet(conf_Get(CONF_URL1));
      if (httpStatus >= 400) {
        led_Set(0x0000ff);
      } else if (httpStatus >= 100) {
        led_Set(0x00ff00);
      } else {
        led_Set(0xffffff);
      }

      printf("HTTP %d\n", httpStatus);

      delay(2000);
      led_Set(0);

      g_buttonPressed = false;
    }
  }

  return 0;
}

void SysTick_Handler(void) {
  uint32_t bpDuration;

  g_SystemTick++;
  if (g_SystemDelay) {
    g_SystemDelay--;
  }

  wifi_SysTickHandler();
  bpDuration = button_PressedDuration();
  led_SysTickHandler(bpDuration);
}

void delay(volatile uint32_t ms) {
  g_SystemDelay = ms;
  while (g_SystemDelay != 0)
    ;
}

static void clockSetup(void) {
  const struct rcc_clock_scale bttn_clock_config = {
      .pll_source = RCC_CFGR_PLLSRC_HSE_CLK, // Use HSE
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

  rcc_clock_setup_pll(&bttn_clock_config);

  systick_set_reload((uint32_t)(CORE_CLOCK / 1000)); // 1ms
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_interrupt_enable();
  systick_counter_enable();
}

static void gpioSetup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);

  gpio_clear(GPIOB, GPIO2); // Boot1 pin
  gpio_clear(GPIOB, GPIO5); // Power switch
  gpio_clear(GPIOC, GPIO7); // Power control
  gpio_clear(GPIOA, GPIO5); // LED8

  // Boot pin
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO5);

  // Power switch
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO2);

  // Power control
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);
  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO7);

  // LED8 (power indicator)
  gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO5);
}
