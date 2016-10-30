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

void enterRecovery(void);
void exitRecovery(void);
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

  if (button_IsPressed()) {
    // Tick speed 800, handler uses 6 ticks and we trigger already on 5th. This
    // gives us recovery after 4000ms (1000 * 5 = 4000ms).
    led_TickConfigure(800, g_SystemTick, &led_TickHandlerRecovery);
    led_TickEnable();

    // Wait until button is released or 4000ms has passed.
    while (button_IsPressed() && button_PressedDuration() < 4000) {
      ;
    }

    led_TickDisable();

    // Recovery mode is entered after the button is pressed for 4000ms or more
    // during initial boot.
    if (button_PressedDuration() >= 4000) {
      enterRecovery();

      while (1) {
        if (wifi_RecoveryOtaRequested()) {
          led_TickConfigure(1000, g_SystemTick,
                            &led_TickHandlerRecoveryLoading);
          led_TickEnable();

          // TODO: Avoid using g_wifiData here, refactor into wifi_ApplyOta()?
          if (!wifi_OtaUpdate(g_wifiData.config->otaUrl)) {
            printf("Failed to perform OTA update on WiFi module!\n");
            led_TickDisable();
            led_TickConfigure(200, g_SystemTick, &led_TickHandlerError);
            led_TickEnable();

            wifi_HardReset(); // Power cycle the Wi-Fi module just in case.

            delay(4000);
          } else {
            led_TickDisable();
            led_Set((0x3F << 8));

            delay(5000);
          }

          led_TickDisable();
          led_Set(0);

          // Restore recovery state after OTA reset.
          enterRecovery();
        }

        if (wifi_RecoveryCommitRequested()) {
          wifi_ApplyConfig();
          conf_HandleChange();
          break;
        }
      }

      exitRecovery();
    }
    led_TickDisable();
    led_Set(0);
  }

  led_TickConfigure(500, g_SystemTick, &led_TickHandlerBoot);
  led_TickEnable();

  wifi_CreateFileInRam("index.html", "text/html", "gzip",
                       (char *)&g_DataIndexHtmlGz[0],
                       DATA_INDEX_HTML_GZ_LENGTH);
  conf_CreateConfigJson();

  printf("Waiting for WIFI UP...\n");
  wifi_WaitState(WIFI_STATE_UP);

  // WIFI is up, power off leds.
  led_TickDisable();
  led_Set(0x3F << 8);
  delay(2500);
  led_Set(0);

  printf("Entering main loop!\n");
  button_Reset();

  while (1) {
    conf_HandleChange();

    if (button_IsPressed() || button_PressedDuration() > 0) {
      uint16_t httpStatus;
      char *url;

      // Show the loading indicator for a long press.
      led_TickConfigure(500, g_SystemTick, &led_TickHandlerGreenCircleFill);
      led_TickEnable();

      // Delay until either button is released or we higt the long-press
      // treshold. 2500 is enough time for all leds to light up, but we add an
      // extra 50ms to give the last led a little more time to shine.
      while (button_IsPressed() && button_PressedDuration() < 2550) {
        ;
      }

      // If the duration matches a long press, use the long press URL.
      if (button_PressedDuration() >= 2550) {
        url = conf_Get(CONF_URL2);
      } else {
        url = conf_Get(CONF_URL1);
      }

      // Speedy loading indicator (rotating light).
      led_TickConfigure(50, g_SystemTick, &led_TickHandlerGreenLoading);
      led_TickEnable();

      httpStatus = wifi_HttpGet(url);
      led_TickDisable();

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

      button_Reset();
    }
  }

  return 0;
}

void enterRecovery(void) {
  printf("Enter recovery!\n");

  led_TickConfigure(500, g_SystemTick, &led_TickHandlerRecoveryInit);
  led_TickEnable();

  wifi_EnableRecovery();
  wifi_EnableFirstConfig("openbttn");
  wifi_CreateFileInRam("firstset.html", "text/html", "gzip",
                       (char *)&g_DataFirstsetHtmlGz[0],
                       DATA_FIRSTSET_HTML_GZ_LENGTH);
  wifi_WaitState(WIFI_STATE_UP);

  led_TickDisable();
  led_Set((0x3F << 16)); // Shine blue.
}

void exitRecovery(void) { wifi_DisableRecovery(); }

void SysTick_Handler(void) {
  g_SystemTick++;
  if (g_SystemDelay) {
    g_SystemDelay--;
  }

  wifi_SysTickHandler();
  button_SysTickHandler();
  led_SysTickHandler(g_SystemTick);
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
