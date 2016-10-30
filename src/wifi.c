#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "util.h"
#include "wifi.h"

uint8_t g_wifiRingBuffSpace[RING_BUFF_SIZE];
uint8_t g_wifiTmpBuff[WIFI_TMP_BUFF_SIZE];
uint8_t g_wifiAtBuff[WIFI_AT_BUFF_SIZE];

RingBuffer g_wifiRingBuff = {&g_wifiRingBuffSpace[0], 0, 0, 0};
WifiAt g_wifiAt = {AT_STATUS_CLEAR, &g_wifiAtBuff[0], 0, 0};
WifiConfig g_wifiConfig;
WifiData g_wifiData;

static void gpioSetup(void);
static void usartSetup(void);
static void atReset(WifiAt *at);
static bool processAsyncIndication(uint8_t *const buff, uint8_t data);
static bool processWind(volatile WifiState *state, uint8_t *const buff);
static bool processAtResponse(WifiAt *at, uint8_t data);
static bool processCind(WifiConfig *wifiConf, uint8_t *const buff);
static uint16_t httpStatus(uint8_t *response);
static void debugPrintBuffer(uint8_t *const buff, uint8_t prefix);

// wifi_Init boots the WIFI module.
void wifi_Init(void) {
  WifiData *wifi = &g_wifiData;

  gpioSetup();
  usartSetup();

  wifi->state = WIFI_STATE_OFF;
  wifi->recv = RECV_ASYNC_INDICATION;
  wifi->at = &g_wifiAt;
  wifi->config = &g_wifiConfig;
  wifi->ringBuff = &g_wifiRingBuff;
  wifi->tmpBuff = &g_wifiTmpBuff[0];

  wifi_PowerOn();
}

void wifi_PowerOn(void) { gpio_set(GPIOB, GPIO2); }
void wifi_PowerOff(void) { gpio_clear(GPIOB, GPIO2); }
void wifi_Send(char data) { usart_send_blocking(WIFI_USART, data); }

// wifi_SoftReset executes the AT+CFUN command, issuing a reset, and waits for
// the power on indication from the WIFI module.
void wifi_SoftReset(void) {
  WifiData *wifi = &g_wifiData;

  // We must wait for the console to be active.
  wifi_WaitState(WIFI_STATE_CONSOLE_ACTIVE);

  // Unset the power on state so that we can wait for it.
  wifi->state &= ~(WIFI_STATE_POWER_ON);

  wifi_SendString("AT+CFUN=1\r"); // Reset wifi module.
  wifi_WaitState(WIFI_STATE_POWER_ON);
}

void wifi_HardReset(void) {
  WifiData *wifi = &g_wifiData;

  wifi_PowerOff();
  delay(1000);
  wifi->state = WIFI_STATE_OFF;
  wifi_PowerOn();
  wifi_WaitState(WIFI_STATE_POWER_ON);
}

// wifi_WaitState waits until the WIFI module is in specified state.
void wifi_WaitState(WifiState states) {
  WifiData *wifi = &g_wifiData;

  while ((wifi->state & states) == 0) {
    ;
  }
}

void wifi_SendString(const char *str) {
  while (*str != '\0') {
    wifi_Send(*str++);
  }
}

// gpioSetup configures the GPIOs for settings up the USART.
static void gpioSetup(void) {
  rcc_periph_clock_enable(RCC_WIFI_USART);

  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, WIFI_GPIO_TX);
  gpio_set_output_options(WIFI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          WIFI_GPIO_TX);

  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, WIFI_GPIO_RX);
  gpio_set_output_options(WIFI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          WIFI_GPIO_RX);

  gpio_set_af(WIFI_GPIO_PORT, GPIO_AF7, WIFI_GPIO_TX);
  gpio_set_af(WIFI_GPIO_PORT, GPIO_AF7, WIFI_GPIO_RX);
}

// usartSetup configures the USART for communicating with the WIFI module.
static void usartSetup(void) {
  usart_set_baudrate(WIFI_USART, 115200);
  usart_set_databits(WIFI_USART, 8);
  usart_set_stopbits(WIFI_USART, USART_STOPBITS_1);
  usart_set_mode(WIFI_USART, USART_MODE_TX_RX);
  usart_set_parity(WIFI_USART, USART_PARITY_NONE);
  usart_set_flow_control(WIFI_USART, USART_FLOWCONTROL_NONE);

  nvic_enable_irq(WIFI_NVIC_IRQ);

  // Give lower priority to SYSTICK IRQ than to WIFI USART IRQ so that we can
  // keep pushing data into the ring buffer even when wifi_SysTickHandler is
  // processing.
  nvic_set_priority(NVIC_SYSTICK_IRQ, (1 << 4));
  nvic_set_priority(WIFI_NVIC_IRQ, (0 << 4));

  usart_enable_rx_interrupt(WIFI_USART);

  usart_enable(WIFI_USART);
}

void wifi_HandleChange(void) {
  WifiData *wifi = &g_wifiData;

  if (wifi->config->changed) {
    wifi_ApplyConfig();
    wifi->config->changed = false;
  }
}

// atReset resets the AT command state.
static void atReset(WifiAt *at) {
  memset(at->buff, 0, WIFI_AT_BUFF_SIZE);
  at->status = AT_STATUS_CLEAR;
  at->last_cr_lf = 0;
  at->pos = 0;
}

// wifi_AtCmdWait waits until we recieve the entire AT response and returns true
// if there was no error, otherwise false.
bool wifi_AtCmdWait(void) {
  WifiData *wifi = &g_wifiData;

  while ((wifi->at->status & AT_STATUS_READY) == 0) {
    ;
  }

  return (wifi->at->status & AT_STATUS_ERROR) == 0;
}

void wifi_AtCmdN(int n, ...) {
  va_list args;
  int i;
  WifiData *wifi = &g_wifiData;

  wifi_WaitState(WIFI_STATE_CONSOLE_ACTIVE);
  atReset(wifi->at);

  va_start(args, n);
  for (i = 0; i < n; i++) {
    wifi_SendString(va_arg(args, const char *));
  }
  va_end(args);

  // Change the recv type before sending the final "\r" to prevent potential
  // race conditions. This works because sending "A" is blocking and the WIFI
  // module queues all asynchronous indications.
  wifi->recv = RECV_AT_RESPONSE;

  wifi_Send('\r');
}

// wifi_AtCmd sends an AT command to the WIFI module without blocking, the
// response will still be available in the AT buffer once it is received.
void wifi_AtCmd(char *str) { wifi_AtCmdN(1, str); }

// wifi_AtCmdBlocking sends an AT command to the WIFI module and blocks until
// it receives an OK or ERROR status. Returns true on OK and false on ERROR.
bool wifi_AtCmdBlocking(char *str) {
  wifi_AtCmdN(1, str);
  return wifi_AtCmdWait();
}

// wifi_HttpGet performs a blocking HTTP GET request and returns the http status
// code returned by the server.
uint16_t wifi_HttpGet(char *url) {
  WifiData *wifi = &g_wifiData;

  wifi_AtCmdN(2, "AT+S.HTTPGET=", url);

  // We use fast processing here to quickly receive the response.
  wifi->at->status |= AT_STATUS_FAST_PROCESS;

  wifi_AtCmdWait();

  if ((wifi->at->status & AT_STATUS_OK) != 0) {
    return httpStatus(wifi->at->buff);
  } else {
    return 0;
  }
}

void wifi_GetSsid(char *dest, size_t len) {
  WifiData *wifi = &g_wifiData;
  char *s;

  assert(len >= 32);

  wifi_AtCmdBlocking("AT+S.GCFG=wifi_ssid");

  s = strstr((const char *)wifi->at->buff, "#  wifi_ssid = ");
  if (s) {
    s += 15; // Skip over "#  wifi_ssid = ".
    int i;
    for (i = 0; i < 32 && isxdigit(*s); i++) { // Max lenght is 32.
      dest[i] = strtol(s, &s, 16);
      if (*s == ':') {
        s++;
      }
    }
  }
}

const char httpFileHeader[] = "HTTP/1.1 200 OK\n"
                              "Server: OpenBttn\n"
                              "Connection: close\n"
                              "Content-Type: %s\n"
                              "Content-Encoding: %s\n"
                              "Content-Length: %d\n\n";

void wifi_CreateFileInRam(const char *name, const char *contentType,
                          const char *contentEnc, const char *data,
                          uint16_t contentLen) {
  // Header length + content type (e.g. text/html or application/json) + content
  // encoding (e.g. gzip or identity) + content length (< 4096).
  char header[strlen(httpFileHeader) + 20 + 8 + 4 + 1];
  char fileSizeStr[5 + 1];
  uint16_t i;
  uint16_t fileSize;

  assert(strlen(contentType) <= 20);
  assert(strlen(contentEnc) <= 8);
  assert(contentLen < WIFI_FILE_MAX_SIZE);

  sprintf(&header[0], httpFileHeader, contentType, contentEnc, contentLen);
  fileSize = strlen(header) + contentLen;

  assert(fileSize <= WIFI_FILE_MAX_SIZE);

  snprintf(&fileSizeStr[0], 5 + 1, ",%d", fileSize);

  // Delete file in case it already exists, a file in RAM cannot be updated.
  wifi_AtCmdN(2, "AT+S.FSD=/", name);
  wifi_AtCmdWait();

  // (Re)Create the file in RAM.
  wifi_AtCmdN(3, "AT+S.FSC=/", name, &fileSizeStr[0]);
  wifi_AtCmdWait();

  // Append all contents to file.
  wifi_AtCmdN(3, "AT+S.FSA=/", name, &fileSizeStr[0]);
  for (i = 0; i < strlen(header); i++) {
    wifi_Send(header[i]);
  }
  for (i = 0; i < contentLen; i++) {
    wifi_Send(data[i]);
  }

  wifi_AtCmdWait();
}

void wifi_EnableFirstConfig(const char *ssid, const char *password) {
  wifi_AtCmdBlocking("AT&F");
  wifi_AtCmdN(2, "AT+S.SSIDTXT=", ssid);
  wifi_AtCmdWait();
  wifi_AtCmdN(2, "AT+S.SCFG=user_desc,", password);
  wifi_AtCmdWait();
  wifi_AtCmdBlocking("AT+S.SCFG=wifi_priv_mode,0");
  wifi_AtCmdBlocking("AT+S.SCFG=wifi_mode,3");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_cgis,1"); // Only output.cgi.

  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_dhcp,2"); // Customise IP.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_ipaddr,192.168.1.1");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_netmask,255.255.255.0");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_gw,192.168.1.1");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_dns,192.168.1.1");

  wifi_AtCmdBlocking("AT&W");
  wifi_SoftReset();
}

// wifi_ApplyConfig.
// TODO: Handle Open and WEP priv_mode.
void wifi_ApplyConfig(void) {
  WifiData *wifi = &g_wifiData;

  wifi_AtCmdBlocking("AT&F");                  // Factory reset.
  wifi_AtCmdBlocking("AT+S.SCFG=wifi_mode,1"); // Station mode.
  wifi_AtCmdN(2, "AT+S.SSIDTXT=", wifi->config->ssid);
  wifi_AtCmdWait();

  if (wifi->config->privMode == 2) { // WPA & WPA2.
    wifi_AtCmdBlocking("AT+S.SCFG=wifi_priv_mode,2");
    wifi_AtCmdN(2, "AT+S.SCFG=wifi_wpa_psk_text,", wifi->config->password);
    wifi_AtCmdWait();
  } else {
    printf("Open and WEP wifi modes not implemented!\n");
  }

  wifi_AtCmdN(2, "AT+S.SCFG=user_desc,", wifi->config->userDesc);
  wifi_AtCmdWait();

  if (wifi->config->dhcp == 1) {
    wifi_AtCmdBlocking("AT+S.SCFG=ip_use_dhcp,1");
  } else {
    char ipAddr[15 + 1];

    wifi_AtCmdBlocking("AT+S.SCFG=ip_use_dhcp,0");

    ip_itoa(&ipAddr[0], wifi->config->ipAddr);
    wifi_AtCmdN(2, "AT+S.SCFG=ip_ipaddr,", &ipAddr[0]);
    wifi_AtCmdWait();

    ip_itoa(&ipAddr[0], wifi->config->ipNetmask);
    wifi_AtCmdN(2, "AT+S.SCFG=ip_netmask,", &ipAddr[0]);
    wifi_AtCmdWait();

    ip_itoa(&ipAddr[0], wifi->config->ipGateway);
    wifi_AtCmdN(2, "AT+S.SCFG=ip_gw,", &ipAddr[0]);
    wifi_AtCmdWait();

    ip_itoa(&ipAddr[0], wifi->config->ipDns);
    wifi_AtCmdN(2, "AT+S.SCFG=ip_dns,", &ipAddr[0]);
    wifi_AtCmdWait();
  }

  wifi_AtCmdBlocking("AT&W"); // Write settings.
  wifi_SoftReset();
}

// wifi_SysTickHandler consumes the wifi ring buffer and processes it according
// to the current wifi_RecvState. Only one char is processed per tick, except
// when AT_STATUS_FAST_PROCESS is enabled.
void wifi_SysTickHandler(void) {
  WifiData *wifi = &g_wifiData;
  uint8_t data;
  bool status;

process_loop:
  // Temporarily disable interrupts, ring buffer is not thread safe.
  __disable_irq();
  data = rb_Pop(wifi->ringBuff);
  __enable_irq();

  if (data != '\0') {
    switch (wifi->recv) {
    // Asynchronous indications can happen at any point except when an AT
    // command is processing, this is the default wifi_RecvState.
    case RECV_ASYNC_INDICATION:
      status = processAsyncIndication(wifi->tmpBuff, data);

      if (status == WIFI_PROCESS_COMPLETE) {
        if (!processWind(&wifi->state, wifi->tmpBuff) &&
            !processCind(wifi->config, wifi->tmpBuff)) {
          printf("Could not process asynchronous indication\n");
        }

        memset(wifi->tmpBuff, 0, WIFI_TMP_BUFF_SIZE);
      }

      break;
    // AT command responses only happen after an AT command has been issued,
    // some only return OK / ERROR whereas others have a response body, ending
    // with OK / ERROR.
    case RECV_AT_RESPONSE:
      status = processAtResponse(wifi->at, data);
      if (status == WIFI_PROCESS_COMPLETE) {
        wifi->recv = RECV_ASYNC_INDICATION;
      } else if ((wifi->at->status & AT_STATUS_FAST_PROCESS) != 0) {
        goto process_loop;
      }
      break;
    // Unhandled recv state.
    default:
      printf("Unknown wifi->recv state\n");
      break;
    }
  }
}

// WIFI_ISR handles interrupts from the WIFI module and stores the data in a
// ring buffer.
void WIFI_ISR(void) {
  WifiData *wifi = &g_wifiData;

  if (usart_get_flag(WIFI_USART, USART_SR_RXNE)) {
    uint8_t data;
    data = usart_recv(WIFI_USART);

    // Temporarily disable interrupts, ring buffer is not thread safe.
    __disable_irq();
    rb_Push(wifi->ringBuff, data);
    __enable_irq();
  }
}

// processAsyncIndication any asynchronous communication from the WIFI
// module, indicating whenever a response is ready to be processed.
static bool processAsyncIndication(uint8_t *const buff, uint8_t data) {
  static uint16_t pos = 0;
  static uint8_t prev = '\0';

  buff[pos++] = data;

  // The beginning and the end of an asynchronous indication is marked by
  // "\r\n", by skipping the first two chars we look for pairs of "\r\n".
  if (pos > 2 && prev == '\r' && data == '\n') {
    debugPrintBuffer(buff, '+');

    pos = 0;
    prev = '\0';
    return WIFI_PROCESS_COMPLETE;
  }

  prev = data;
  return WIFI_PROCESS_INCOMPLETE;
}

// processAtResponse processes the data in the provided at buffer and
// indicates whether or not the entire response has been received, the AT status
// is updated accordingly.
static bool processAtResponse(WifiAt *at, uint8_t data) {
  at->buff[at->pos++] = data;

  // A response must always end at a "\r\n", by skipping len under the minimum
  // response length we avoid unecessary processing in the beginning.
  if (data == '\n' && at->buff[at->pos - 2] == '\r') {
    if (at->last_cr_lf != 0) {
      // Check for AT OK response (end), indicating a successfull HTTP request.
      if (strstr((const char *)at->last_cr_lf, "\r\nOK\r\n")) {
        debugPrintBuffer(at->buff, '#');
        at->status = AT_STATUS_OK | AT_STATUS_READY;

        return WIFI_PROCESS_COMPLETE;
      }

      // Check for AT error response (end), indicating there was an error. We
      // check from last_cr_lf to ensure we get the full error message.
      if (strstr((const char *)at->last_cr_lf, "\r\nERROR")) {
        debugPrintBuffer(at->buff, '!');
        at->status = AT_STATUS_ERROR | AT_STATUS_READY;

        return WIFI_PROCESS_COMPLETE;
      }
    }

    // Keep track of the current "\r\n" position.
    at->last_cr_lf = &at->buff[at->pos - 2];
  }

  return WIFI_PROCESS_INCOMPLETE;
}

// processWind consumes a buffer containing WIND and updates the state if
// a handled WIND ID is found.
static bool processWind(volatile WifiState *state, uint8_t *const buff) {
  WifiWind wind = WIND_UNDEFINED;

  char *windStart = strstr((const char *)buff, "+WIND:");
  if (windStart) {
    windStart += 6; // Skip over "+WIND:", next char is a digit.

    // We assume the indication ID is never greater than 99 (two digits).
    wind = *windStart++ - '0'; // Convert char to int
    if (*windStart != ':') {
      wind *= 10;               // First digit was a multiple of 10
      wind += *windStart - '0'; // Convert char to int
    }
  }

  switch (wind) {
  case WIND_POWER_ON:
    *state = WIFI_STATE_POWER_ON; // Reset WIFI state after power on.
    break;
  case WIND_RESET:
    *state = WIFI_STATE_OFF;
    break;
  case WIND_CONSOLE_ACTIVE:
    *state |= WIFI_STATE_CONSOLE_ACTIVE;
    break;
  case WIND_WIFI_ASSOCIATED:
    *state |= WIFI_STATE_ASSOCIATED;
    break;
  case WIND_WIFI_JOINED:
    *state |= WIFI_STATE_JOINED;
    break;
  case WIND_WIFI_UP:
    *state |= WIFI_STATE_UP;
    break;
  case WIND_UNDEFINED:
    return false;
    break;
  }

  return true;
}

// processCind consumes a buffer containing CIND (custom indication) and
// enables remote management of the bttn.
static bool processCind(WifiConfig *wifiConfig, uint8_t *const buff) {
  char text[WIFI_CIND_TEXT_LENGTH];
  char *const pText = &text[0];
  WifiCind cind = CIND_UNDEF;

  char *cindStart = strstr((const char *)buff, "+CIND:");
  if (cindStart) {
    cindStart += 6; // Skip over "+CIND:", next char is a digit.

    // We assume the indication ID is never greater than 99 (two digits).
    cind = *cindStart++ - '0'; // Convert char to int
    if (*cindStart != ':') {
      cind *= 10;                 // First digit was a multiple of 10
      cind += *cindStart++ - '0'; // Convert char to int
    }

    cindStart++; // Skip over ':', leaving message.
  }

  if (cind != CIND_UNDEF) {
    if (cind == CIND_AUTHENTICATE || conf_IsUnlocked()) {
      url_decode(pText, cindStart, WIFI_CIND_MESSAGE_LENGTH);
      debugPrintBuffer((uint8_t *)pText, 'C');
    } else {
      printf("Received non-authenticated CIND, ignoring...\n");
      return true; // We processed the CIND, but it was unauthorised.
    }
  }

  switch (cind) {
  case CIND_AUTHENTICATE:
    conf_Unlock(pText);
    break;
  case CIND_COMMIT_CONFIG:
    conf_RequestCommit();
    break;
  case CIND_SET_URL1:
    conf_Set(CONF_URL1, pText);
    break;
  case CIND_SET_URL2:
    conf_Set(CONF_URL2, pText);
    break;
  case CIND_SET_USER_DESC:
    memset(wifiConfig->userDesc, 0, WIFI_CONFIG_USER_DESC_LENGTH);
    strncpy(wifiConfig->userDesc, pText, WIFI_CONFIG_USER_DESC_LENGTH);
    conf_Set(CONF_PASSWORD, pText);
    break;
  case CIND_SET_SSID:
    memset(wifiConfig->ssid, 0, WIFI_CONFIG_SSID_LENGTH);
    strncpy(wifiConfig->ssid, pText, WIFI_CONFIG_SSID_LENGTH);
    break;
  case CIND_SET_PASSWORD:
    memset(wifiConfig->password, 0, WIFI_CONFIG_PASSWORD_LENGTH);
    strncpy(wifiConfig->password, pText, WIFI_CONFIG_PASSWORD_LENGTH);
    break;
  case CIND_SET_PRIV_MODE:
    wifiConfig->privMode = (uint8_t)atoi(pText);
    break;
  case CIND_SET_DHCP:
    wifiConfig->dhcp = (uint8_t)atoi(pText);
    break;
  case CIND_SET_IP_ADDR:
    wifiConfig->ipAddr = ip_atoi(pText);
    break;
  case CIND_SET_IP_NETMASK:
    wifiConfig->ipNetmask = ip_atoi(pText);
    break;
  case CIND_SET_IP_GATEWAY:
    wifiConfig->ipGateway = ip_atoi(pText);
    break;
  case CIND_SET_IP_DNS:
    wifiConfig->ipDns = ip_atoi(pText);
    break;
  case CIND_WIFI_COMMIT:
    wifiConfig->changed = true;
    break;
  case CIND_UNDEF:
    return false;
    break;
  }

  return true;
}

// httpStatus takes a http response buffer and tries to parse the
// status code from the http header, zero is returned when no status is found.
static uint16_t httpStatus(uint8_t *response) {
  uint16_t status = 0;
  char status_str[3];
  char *header_ptr;

  header_ptr = strstr((const char *)response, "HTTP/1.");
  if (header_ptr) {
    // The status code is the 9th-11th element of the header.
    // Example: "HTTP/1.0 200 OK"
    memcpy(&status_str, (header_ptr + 9), 3);
    status = atoi(&status_str[0]);
  }

  return status;
}

static void debugPrintBuffer(uint8_t *const buff, uint8_t prefix) {
  uint8_t *ptr = &buff[0];

  debug_Send(prefix);
  debug_Send('>');

  while (*ptr) {
    if (*ptr == '\r') {
      debug_Send('\\');
      debug_Send('r');
    } else if (*ptr == '\n') {
      debug_Send('\\');
      debug_Send('n');
    } else {
      debug_Send(*ptr);
    }
    ptr++;
  }

  debug_Send('\r');
  debug_Send('\n');
}
