#include <stdio.h>
#include <string.h>

#include "button.h"
#include "conf.h"
#include "debug.h"
#include "led.h"
#include "syscfg.h"
#include "wifi.h"

volatile uint32_t system_delay = 0;

static void clock_setup(void);
static void gpio_setup(void);

int main(void) {
  clock_setup();

  gpio_setup();

  // Power on LED8.
  gpio_set(GPIOA, GPIO5);

  // Enable board power (power switch).
  gpio_set(GPIOB, GPIO5);
  delay(50);
  // Allow power to board (power control).
  gpio_set(GPIOC, GPIO7);
  delay(50);

  debug_init();
  wifi_Init();

  button_enable();

  leds_init();
  leds_set_brightness(119, 119, 119);

  printf("\nBootup complete!\n");

  // Show blue light until WIFI is ready.
  leds_shift(0xff0000);

  spwf_WaitState(SPWF_STATE_CONSOLE_ACTIVE);
  printf("Configuring WIFI module...\n");

  conf_Load(&config);
  wifi_CreateOpenBTTNPage();
  wifi_StoreConfigJSON(config.data);

  char ssid[32];
  wifi_GetSSID(&ssid[0], sizeof(ssid));
  printf("SSID: %s\n", ssid);

  if (strcmp(&ssid[0], "MY_SSID") != 0) {
    spwf_ATCmdBlocking("AT&F");  // Factory reset

    spwf_ATCmdBlocking("AT+S.SCFG=console1_hwfc,0");  // Hardware flow
                                                      // control does not
                                                      // seem to work.
    spwf_ATCmdBlocking("AT+S.SCFG=console1_errs,2");  // Display error codes.
    spwf_ATCmdBlocking("AT+S.SSIDTXT=MY_SSID");
    spwf_ATCmdBlocking("AT+S.SCFG=wifi_wpa_psk_text,MY_PASSWORD");
    spwf_ATCmdBlocking("AT+S.SCFG=wifi_priv_mode,2");
    spwf_ATCmdBlocking("AT+S.SCFG=wifi_mode,1");
    spwf_ATCmdBlocking("AT+S.SCFG=ip_use_dhcp,1");
    spwf_ATCmdBlocking("AT+S.SCFG=ip_use_decoder,2");
    spwf_ATCmdBlocking("AT&W");  // Write settings
    spwf_SoftReset();
  }

  printf("Waiting for WIFI UP...\n");
  spwf_WaitState(SPWF_STATE_UP);

  // WIFI is up, power off leds.
  leds_shift(0);

  printf("Entering main loop!\n");

  uint16_t httpStatus = 0;
  while (1) {
    conf_HandleChange(&config);

    if (button_pressed) {
      httpStatus = wifi_HTTPGet(&config.data->url1[0]);
      if (httpStatus >= 400) {
        leds_shift(0x0000ff);
      } else if (httpStatus >= 100) {
        leds_shift(0x00ff00);
      } else {
        leds_shift(0xffffff);
      }

      printf("HTTP %d\n", httpStatus);

      delay(2000);
      leds_shift(0);

      button_pressed = false;
    }
  }

  return 0;
}

static void clock_setup(void) {
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

  gpio_clear(GPIOB, GPIO2);  // Boot1 pin
  gpio_clear(GPIOB, GPIO5);  // Power switch
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
