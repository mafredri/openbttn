#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

static char g_httpHeader[HTTP_HEADER_LENGTH];

typedef struct SockRequest SockRequest;
struct SockRequest {
  bool auth;
  bool saveConfig;
  bool dumpConfig;
};

static bool parseParamValue(char **dest, char *const src, char *param);
static bool processSockRequest(SockRequest *request, char *data);

typedef struct RecoverySockRequest RecoverySockRequest;
struct RecoverySockRequest {
  WifiConfig *wifiConfig;
  char otaUrl[WIFI_URL_LENGTH + 1];
  bool applyConfig;
  bool otaUpdate;
};
static bool shouldEnterRecovery(void);
static void enterRecoveryMain(void);
static void initRecoveryMode(void);
static bool processRecoverySockRequest(RecoverySockRequest *request,
                                       char *data);
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

  if (shouldEnterRecovery()) { // Press button for 4s during startup.
    enterRecoveryMain();       // Exit by system reset.
  }

  led_TickConfigure(500, g_SystemTick, &led_TickHandlerBoot);
  led_TickEnable();

  wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 200, "OK",
                        "text/html", "gzip", DATA_INDEX_HTML_GZ_LENGTH);
  wifi_CreateFileInRam("index.html", g_httpHeader, (char *)g_DataIndexHtmlGz,
                       DATA_INDEX_HTML_GZ_LENGTH);

  printf("Waiting for WIFI UP...\n");
  wifi_WaitState(WIFI_STATE_UP);

  // Indicate successfull bootup by displaying green leds for 2.5 seconds.
  led_TickDisable();
  led_Set(0x3F << 8);
  delay(2500);
  led_Set(0);

  button_Reset(); // Reset button before main loop.

  printf("Entering main loop!\n");

  char *socketData = NULL;
  while (true) {
    // Make sure the socket server is running.
    if (!wifi_SockdStarted()) {
      wifi_StartSockd(SOCKD_PORT);
    }
    // Handle state changes for the socket server.
    wifi_SockdHandler();

    socketData = (char *)wifi_SockdGetData();
    if (socketData != NULL) {
      SockRequest req = {0};
      bool err = false;

      err = processSockRequest(&req, socketData);
      wifi_SockdClearData();

      if (err) {
        const char msg[] = "Could not process request";
        wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 400,
                              "Bad Request", "text/plain", NULL, strlen(msg));
        wifi_SockdSendN(2, g_httpHeader, msg);
        continue;
      }

      if (!req.auth) {
        const char msg[] = "Authentication failed!";
        wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 403,
                              "Forbidden", "text/plain", NULL, strlen(msg));
        wifi_SockdSendN(2, g_httpHeader, msg);
        continue;
      }

      wifi_SockdIsSafeClient();

      if (req.dumpConfig) {
        char *url1 = conf_Get(CONF_URL1);
        char *url2 = conf_Get(CONF_URL2);
        uint16_t len =
            21 + strlen(url1) + strlen(url2); // {"url1":"","url2":""}
        wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 200, "OK",
                              "text/plain", NULL, len);
        wifi_SockdSendN(6, g_httpHeader, "{\"url1\":\"", url1, "\",\"url2\":\"",
                        url2, "\"}");
        continue;
      }

      if (req.saveConfig) {
        conf_Save();
      }

      const char msg[] = "Success!";
      wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 200, "OK",
                            "text/plain", NULL, strlen(msg));
      wifi_SockdSendN(2, g_httpHeader, msg);
    }

    if (button_IsPressed() || button_PressedDuration() > 0) {
      uint16_t httpStatus = 0;
      char *url = NULL;

      // Show the loading indicator for a long press.
      led_TickConfigure(500, g_SystemTick, &led_TickHandlerGreenCircleFill);
      led_TickEnable();

      // Delay until either button is released or we higt the long-press
      // treshold. 2500 is enough time for all leds to light up, but we add an
      // extra 50ms to give the last led a little more time to shine.
      while (button_IsPressed() && button_PressedDuration() < 2550)
        ;

      // If the duration matches a long press, use the long press URL.
      if (button_PressedDuration() >= 2550) {
        url = conf_Get(CONF_URL2);
      } else {
        url = conf_Get(CONF_URL1);
      }

      // Speedy loading indicator (rotating light).
      led_SetBrightness(69, 119, 1);
      led_TickConfigure(70, g_SystemTick, &led_TickHandlerGreenLoading);
      led_TickEnable();

      httpStatus = wifi_HttpGet(url);
      led_TickDisable();
      led_SetBrightness(119, 119, 119);

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

// parseParamValue parses a string of type "param = value", returns true if the
// requested param was found and sets the value to dest.
static bool parseParamValue(char **dest, char *const src, char *param) {
  uint8_t len = strlen(param);
  if (strncasecmp(src, param, len) == 0 && strncmp(src + len, " = ", 3) == 0) {
    *dest = src + len + 3;
    return true;
  }

  return false;
}

// processSockRequest processes the data from a socket request and stores the
// result in a SockRequest. First parameter must be authentication, otherwise
// the request is rejected.
static bool processSockRequest(SockRequest *req, char *data) {
  bool error = false;
  char *pch = NULL;
  char *value = NULL;

  // Tokenise the data on newlines, each new line represents a new parameter /
  // value.
  pch = strtok(data, "\r\n");

  // First parameter must be authentication.
  if (parseParamValue(&value, pch, "auth")) {
    if (strncmp((char *)conf_Get(CONF_PASSWORD), value, CONF_PASSWORD_LENGTH) ==
        0) {
      req->auth = true;
    }
  }
  pch = strtok(NULL, "\r\n"); // Next token (after auth).

  while (req->auth && pch != NULL) {
    value = NULL;

    if (strncmp(pch, "dump_config", 12) == 0) {
      req->dumpConfig = true;
      // Accept no other parameters after dump_config.
      break;
    } else if (parseParamValue(&value, pch, "blink_leds")) {
      char *pNum;
      uint32_t num;
      bool isDelay = false;
      bool hasNext = true;

      while (hasNext) {
        pNum = value;

        while (isxdigit(*value++))
          ;
        if (pNum == value) {
          break;
        }

        num = (uint32_t)strtoul(pNum, NULL, 16);
        if (isDelay) {
          delay(num);
        } else {
          led_Set(num);
        }

        isDelay = !isDelay;
        hasNext = (*(value - 1) == ';');
      }
    } else if (parseParamValue(&value, pch, "url1")) {
      conf_Set(CONF_URL1, value);
      req->saveConfig = true;
    } else if (parseParamValue(&value, pch, "url2")) {
      conf_Set(CONF_URL2, value);
      req->saveConfig = true;
    } else if (parseParamValue(&value, pch, "password")) {
      conf_Set(CONF_PASSWORD, value);
      req->saveConfig = true;
    } else {
      error = true;
      break;
    }

    pch = strtok(NULL, "\r\n");
  }

  return error;
}

// shouldEnterRecovery returns true if the button has been pressed for 4 seconds
// (or more) during startup, false otherwise.
static bool shouldEnterRecovery(void) {
  if (!button_IsPressed()) {
    return false;
  }

  // Tick speed 800, handler uses 6 ticks and we trigger already on 5th. This
  // gives us recovery after 4000ms (1000 * 5 = 4000ms).
  led_TickConfigure(800, g_SystemTick, &led_TickHandlerRecovery);
  led_TickEnable();

  // Wait until button is released or 4000ms has passed.
  while (button_IsPressed() && button_PressedDuration() < 4000)
    ;

  led_TickDisable();

  if (button_PressedDuration() < 4000) {
    // Button was pressed <4s, reset leds and resume normal boot.
    led_Set(0);
    return false;
  }

  // Button was pressed for 4s, enter recovery mode.
  return true;
}

// enterRecoveryMain enters the main loop for recovery mode.
static void enterRecoveryMain(void) {
  char *socketData = NULL;

  printf("Entering recovery mode!\n");

  initRecoveryMode(); // Initialise recovery mode.

  // Recovery loop, can only be exited by system reset.
  while (true) {
    // Make sure the socket server is running.
    if (!wifi_SockdStarted()) {
      wifi_StartSockd(SOCKD_PORT);
    }
    // Handle state changes for the socket server.
    wifi_SockdHandler();

    socketData = (char *)wifi_SockdGetData();
    if (socketData != NULL) {
      WifiConfig wifiConfig = {0};
      RecoverySockRequest req = {0};
      req.wifiConfig = &wifiConfig;
      bool err = false;

      err = processRecoverySockRequest(&req, socketData);
      wifi_SockdIsSafeClient();
      wifi_SockdClearData(); // Data has been processed.

      if (err) {
        const char msg[] = "Could not process request";
        wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 400,
                              "Bad Request", "text/plain", NULL, strlen(msg));
        wifi_SockdSendN(2, g_httpHeader, msg);
      } else if (req.otaUpdate) {
        led_TickConfigure(1000, g_SystemTick, &led_TickHandlerRecoveryLoading);
        led_TickEnable();

        if (wifi_OtaDownload(req.otaUrl)) {
          const char msg[] = "OTA update downloaded successfully, rebooting "
                             "to finalise setup";
          wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 200, "OK",
                                "text/plain", NULL, strlen(msg));
          wifi_SockdSendN(2, g_httpHeader, msg);

          led_TickDisable();
          if (wifi_OtaComplete()) {
            led_Set((0x3F << 8)); // Indicate success through LEDs.
          } else {
            printf("Failed to perform OTA update on WiFi module!\n");

            // Indicate error through LEDs.
            led_TickConfigure(200, g_SystemTick, &led_TickHandlerError);
            led_TickEnable();
          }
        } else {
          const char msg[] = "Error downloading OTA update";
          printf("Failed to download OTA update for WiFi module!\n");
          led_TickDisable();

          // Indicate error through LEDs.
          led_TickConfigure(200, g_SystemTick, &led_TickHandlerError);
          led_TickEnable();

          wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 500,
                                "Internal Server Error", "text/plain", NULL,
                                strlen(msg));
          wifi_SockdSendN(2, g_httpHeader, msg);

          delay(5000); // Show error status for 5000 ms.
          led_TickDisable();
          led_Set((0x3F << 16)); // Restore recovery (blue) LEDs.
          continue;              // Restart recovery loop (no restart happened).
        }

        delay(5000); // Show status for 5000 ms.
        led_TickDisable();
        led_Set(0);
        initRecoveryMode(); // Restore recovery state after OTA reset.
      } else if (req.applyConfig) {
        const char msg[] = "Configuration applied! Rebooting...";

        // We must respond to the user before applying the configuration,
        // otherwise the network communication will not work.
        wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 200, "OK",
                              "text/plain", NULL, strlen(msg));
        wifi_SockdSendN(2, g_httpHeader, msg);

        // Apply the user provided configuration.
        wifi_ApplyConfig(req.wifiConfig);

        scb_reset_system(); // Perform full system reset.
      }
    }
  }
}

// initRecoveryMode initialises recovery mode by setting up the OpenBttn
// hotspot, creating the configuration page and indicating status through
// LEDs.
static void initRecoveryMode(void) {
  led_TickConfigure(500, g_SystemTick, &led_TickHandlerRecoveryInit);
  led_TickEnable();

  wifi_EnableFirstConfig("OpenBttn");
  wifi_CreateHttpHeader(g_httpHeader, HTTP_HEADER_LENGTH, 200, "OK",
                        "text/html", "gzip", DATA_FIRSTSET_HTML_GZ_LENGTH);
  wifi_CreateFileInRam("firstset.html", g_httpHeader,
                       (char *)g_DataFirstsetHtmlGz,
                       DATA_FIRSTSET_HTML_GZ_LENGTH);

  wifi_WaitState(WIFI_STATE_UP);

  led_TickDisable();
  led_Set((0x3F << 16)); // Shine blue.
}

// processRecoverySockRequest processess socket requests in recovery mode.
static bool processRecoverySockRequest(RecoverySockRequest *req, char *data) {
  bool error = false;
  char *pch = NULL;
  char *value = NULL;

  // Tokenise the data on newlines, each new line represents a new parameter /
  // value.
  pch = strtok(data, "\r\n");

  while (pch != NULL) {
    value = NULL;

    if (parseParamValue(&value, pch, "password")) {
      conf_Set(CONF_PASSWORD, value);
      conf_Save();
      strncpy(req->wifiConfig->userDesc, conf_Get(CONF_PASSWORD),
              sizeof req->wifiConfig->userDesc);
    } else if (parseParamValue(&value, pch, "ssid")) {
      strncpy(req->wifiConfig->ssid, value, sizeof req->wifiConfig->ssid);

      // Arbitrarily, we pick SSID as an indication that config have changed.
      req->applyConfig = true;
    } else if (parseParamValue(&value, pch, "wpa_psk")) {
      strncpy(req->wifiConfig->wpaPsk, value, sizeof req->wifiConfig->wpaPsk);
    } else if (parseParamValue(&value, pch, "priv_mode")) {
      req->wifiConfig->privMode = (uint8_t)atoi(value);
    } else if (parseParamValue(&value, pch, "wifi_mode")) {
      req->wifiConfig->wifiMode = (uint8_t)atoi(value);
    } else if (parseParamValue(&value, pch, "dhcp")) {
      req->wifiConfig->dhcp = (uint8_t)atoi(value);
    } else if (parseParamValue(&value, pch, "ip_addr")) {
      strncpy(req->wifiConfig->ipAddr, value, sizeof req->wifiConfig->ipAddr);
    } else if (parseParamValue(&value, pch, "ip_netmask")) {
      strncpy(req->wifiConfig->ipNetmask, value,
              sizeof req->wifiConfig->ipNetmask);
    } else if (parseParamValue(&value, pch, "ip_gateway")) {
      strncpy(req->wifiConfig->ipGateway, value,
              sizeof req->wifiConfig->ipGateway);
    } else if (parseParamValue(&value, pch, "ip_dns")) {
      strncpy(req->wifiConfig->ipDns, value, sizeof req->wifiConfig->ipDns);
    } else if (parseParamValue(&value, pch, "ota")) {
      strncpy(req->otaUrl, value, sizeof req->otaUrl);
      req->otaUpdate = true;
    } else {
      error = true;
      break;
    }

    pch = strtok(NULL, "\r\n");
  }

  return error;
}

// SysTick_Handler handles the system and calls maintenance functions for wifi,
// button and led.
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

// clockSetup configures the bttn clock speed according to CORE_CLOCK.
static void clockSetup(void) {
  const struct rcc_clock_scale bttnClockConfig = {
      .pll_source = RCC_CFGR_PLLSRC_HSE_CLK, // Use HSE
      .pll_mul = RCC_CFGR_PLLMUL_MUL8,
      .pll_div = RCC_CFGR_PLLDIV_DIV2,
      .hpre = RCC_CFGR_HPRE_SYSCLK_NODIV,
      .ppre1 = RCC_CFGR_PPRE1_HCLK_NODIV,
      .ppre2 = RCC_CFGR_PPRE2_HCLK_NODIV,
      .voltage_scale = PWR_SCALE1,
      .flash_waitstates = 1,
      .ahb_frequency = CORE_CLOCK,
      .apb1_frequency = CORE_CLOCK,
      .apb2_frequency = CORE_CLOCK,
  };

  rcc_clock_setup_pll(&bttnClockConfig);

  systick_set_reload((uint32_t)(CORE_CLOCK / 1000)); // 1 ms
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_interrupt_enable();
  systick_counter_enable();
}

// gpioSetup prepares the general GPIOs on the bttn.
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
