#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/assert.h>

#include "debug.h"
#include "wifi.h"

uint8_t g_wifiRingBuffSpace[RING_BUFF_SIZE + 1];
uint8_t g_wifiAtBuffSpace[WIFI_AT_BUFF_SIZE + 1];
uint8_t g_wifiTmpBuffSpace[WIFI_TMP_BUFF_SIZE + 1];
uint8_t g_wifiSocketBuffSpace[WIFI_SOCK_BUFF_SIZE + 1];
uint8_t g_wifiAtCmdBuff[512 + 1];

RingBuffer g_wifiRingBuff = {&g_wifiRingBuffSpace[0], 0, 0, 0};
WifiAt g_wifiAt = {AT_STATUS_CLEAR, &g_wifiAtBuffSpace[0], 0, 0};
WifiBuff g_wifiTmpBuff = {&g_wifiTmpBuffSpace[0], WIFI_TMP_BUFF_SIZE, 0};
WifiBuff g_wifiSocketBuff = {&g_wifiSocketBuffSpace[0], WIFI_SOCK_BUFF_SIZE, 0};
WifiData g_wifiData;

static void gpioSetup(void);
static void usartSetup(void);
static void resetWifiData(WifiData *wifi);
static void atReset(WifiAt *at);
static void atCmdPrepare(void);
void enterCommandMode(void);
void enterDataMode(void);
static inline void safeAppendBuffer(WifiBuff *b, uint8_t data);
static inline void clearBuffer(WifiBuff *b);
static inline bool checkBufferCrLf(WifiBuff *b);
static void guardBufferOverflow(WifiBuff *b);
static bool processWind(volatile WifiState *state, uint8_t *const buff);
static bool processAtResponse(WifiAt *at, uint8_t data);
static uint16_t httpStatus(uint8_t *response);
static void debugPrintBuffer(uint8_t *const buff, uint8_t prefix);

// wifi_Init boots the WIFI module.
void wifi_Init(void) {
  WifiData *wifi = &g_wifiData;

  resetWifiData(wifi);
  gpioSetup();
  usartSetup();

  wifi_PowerOn();
}

void wifi_PowerOn(void) { gpio_set(GPIOB, GPIO2); }
void wifi_PowerOff(void) { gpio_clear(GPIOB, GPIO2); }
void wifi_Send(char data) { usart_send_blocking(WIFI_USART, data); }

// wifi_SoftReset executes the "AT+CFUN=1" command, issuing a reset, and waits
// for the power on indication from the WIFI module.
void wifi_SoftReset(void) {
  WifiData *wifi = &g_wifiData;

  // We must wait for the console to be active.
  wifi_WaitState(WIFI_STATE_CONSOLE_ACTIVE);

  atCmdPrepare();

  wifi_SendString("AT+CFUN=1");
  wifi->state &= ~(WIFI_STATE_POWER_ON);
  wifi_Send('\r');
  wifi_WaitState(WIFI_STATE_POWER_ON);
}

void wifi_HardReset(void) {
  WifiData *wifi = &g_wifiData;

  wifi_PowerOff();
  delay(1000);
  resetWifiData(wifi);
  wifi_PowerOn();
  wifi_WaitState(WIFI_STATE_POWER_ON);
}

void wifi_HandleState(void) {
  WifiData *wifi = &g_wifiData;

  if (wifi->state & WIFI_STATE_SOCKD_PENDING_DATA) {
    enterDataMode();
  }
}

// wifi_WaitState waits until the WIFI module is in specified state.
void wifi_WaitState(WifiState states) {
  WifiData *wifi = &g_wifiData;

  while (wifi->processing)
    ;

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
  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  WIFI_GPIO_TX | WIFI_GPIO_RX);
  gpio_set_output_options(WIFI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          WIFI_GPIO_TX | WIFI_GPIO_RX);
  gpio_set_af(WIFI_GPIO_PORT, GPIO_AF7, WIFI_GPIO_TX | WIFI_GPIO_RX);

  // We setup CTS/RTS but we will not be able to use them due to the incorrect
  // hardware design (bt.tn RevB 3).
  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
                  WIFI_GPIO_CTS);
  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  WIFI_GPIO_RTS);
  gpio_set_output_options(WIFI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                          WIFI_GPIO_CTS | WIFI_GPIO_RTS);
}

// usartSetup configures the USART for communicating with the WIFI module.
static void usartSetup(void) {
  rcc_periph_clock_enable(RCC_WIFI_USART);

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

// resetWifiData resets all of the wifi data state.
static void resetWifiData(WifiData *wifi) {
  wifi->state = WIFI_STATE_OFF;

  // Ring buffer.
  wifi->ringBuff = &g_wifiRingBuff;
  rb_Flush(wifi->ringBuff);

  // AT command state / buffer.
  wifi->at = &g_wifiAt;
  atReset(wifi->at);

  // TMP buffer for asynchronous indications.
  wifi->tmpBuff = &g_wifiTmpBuff;
  clearBuffer(wifi->tmpBuff);

  // Socket buffer for incoming data from SOCKD.
  wifi->sockBuff = &g_wifiSocketBuff;
  clearBuffer(wifi->sockBuff);

  wifi->processing = false;
  wifi->fastProcess = false;
  wifi->reqEnterDataMode = false;
  wifi->reqExitDataMode = false;
  wifi->recv = RECV_ASYNC_INDICATION;
}

// atReset resets the AT command state.
static void atReset(WifiAt *at) {
  memset(at->buff, 0, WIFI_AT_BUFF_SIZE);
  at->status = AT_STATUS_CLEAR;
  at->last_cr_lf = 0;
  at->pos = 0;
}

// atCmdPrepare ensures that it is safe to issue an AT command.
static void atCmdPrepare(void) {
  WifiData *wifi = &g_wifiData;

  wifi_WaitState(WIFI_STATE_CONSOLE_ACTIVE);

#if false
  if (!(wifi->state & WIFI_STATE_SOCKD_CLIENT_ACTIVE)) {
    // Because of a bug in the WiFi module firmware, the socket server cannot
    // be active while we perform an AT command and there is no client
    // connected. If a client were to connect while we are performing the AT
    // command, the AT command will not complete before the client has
    // disconnected.
    wifi_StopSockd();
  }

  enterCommandMode();
#else
  // The WiFi firmware has a bug where the socket data can be output before any
  // WINDs if a client disconnects at the right time, with all four socket WINDs
  // following thereafter. Even though it would be nice not to disconnect
  // clients here, we cannot guarantee that a client won't disconnect between
  // entering command mode and performing the AT command. This could give
  // another client a chance to connect during this window and corrupt our AT
  // command.
  wifi_StopSockd();
#endif
}

// wifi_AtCmdWait waits until we recieve the entire AT response and returns true
// if there was no error, otherwise false.
bool wifi_AtCmdWait(void) {
  WifiData *wifi = &g_wifiData;

  while (!(wifi->at->status & AT_STATUS_READY))
    ;

  return (wifi->at->status & AT_STATUS_ERROR) == 0;
}

// wifi_AtCmdN takes multiple arguments (char *) and issues them as one whole AT
// command to the WiFi module.
void wifi_AtCmdN(int n, ...) {
  va_list args;
  int i;
  WifiData *wifi = &g_wifiData;

  atCmdPrepare();

  // Reset AT after function calls.
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

const char httpHeaderTpl[] = "HTTP/1.0 %d %s\n"
                             "Server: OpenBttn\n"
                             "Content-Type: %s\n"
                             "%s" // "Content-Encoding: %s\n"
                             "Content-Length: %d\n\n";

int wifi_CreateHttpHeader(char *dest, int len, int status,
                          const char *statusText, const char *contentType,
                          const char *contentEnc, uint16_t contentLength) {
  char contentEncoding[HTTP_HEADER_ENCODING_LENGTH + 1] = {0};
  if (contentEnc) {
    int s = snprintf(contentEncoding, HTTP_HEADER_ENCODING_LENGTH,
                     "Content-Encoding: %s\n", contentEnc);
    if (s < 0) {
      return s;
    }
  }
  return snprintf(dest, len, httpHeaderTpl, status, statusText, contentType,
                  contentEncoding, contentLength);
}

void wifi_CreateFileInRam(const char *name, const char *header,
                          const char *body, uint16_t bodyLength) {
  char fileSizeStr[5 + 1];
  uint16_t i;
  uint16_t fileSize = strlen(header) + bodyLength;

  cm3_assert(fileSize <= WIFI_FILE_MAX_SIZE);

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
  for (i = 0; i < bodyLength; i++) {
    wifi_Send(body[i]);
  }

  wifi_AtCmdWait();
}

void wifi_EnableFirstConfig(const char *ssid) {
  wifi_AtCmdBlocking("AT&F");
  wifi_AtCmdN(2, "AT+S.SSIDTXT=", ssid);
  wifi_AtCmdWait();

  wifi_AtCmdBlocking("AT+S.SCFG=console1_hwfc,0");
  wifi_AtCmdBlocking("AT+S.SCFG=wifi_mode,3");      // MiniAP mode.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_decoder,0"); // Use RAW for output.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_cgis,0");    // Disable all CGIs.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_ssis,0");    // Disable all SSIs.
  wifi_AtCmdBlocking("AT+S.SCFG=wifi_priv_mode,0");

  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_dhcp,2"); // Customise IP.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_ipaddr,192.168.1.1");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_netmask,255.255.255.0");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_gw,192.168.1.1");
  wifi_AtCmdBlocking("AT+S.SCFG=ip_dns,192.168.1.1");

  wifi_AtCmdBlocking("AT&W");
  wifi_SoftReset();
}

// wifi_ApplyConfig applies a wifi configuration to the WiFi module, reboot must
// be performed for all changes to take effect.
// TODO: Handle Open and WEP priv_mode.
void wifi_ApplyConfig(WifiConfig *config) {
  wifi_AtCmdBlocking("AT&F"); // Factory reset.

  wifi_AtCmdBlocking("AT+S.SCFG=console1_hwfc,0");
  wifi_AtCmdBlocking("AT+S.SCFG=wifi_mode,1");   // Station mode.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_cgis,0"); // Disable all CGIs.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_ssis,0"); // Disable all SSIs.
  wifi_AtCmdBlocking("AT+S.SCFG=ip_use_decoder,0");

  // Set the SSID we should to connect to.
  wifi_AtCmdN(2, "AT+S.SSIDTXT=", config->ssid);
  wifi_AtCmdWait();

  if (config->privMode == 2) {
    // Enable WPA & WPA2 privacy mode.
    wifi_AtCmdBlocking("AT+S.SCFG=wifi_priv_mode,2");
    wifi_AtCmdN(2, "AT+S.SCFG=wifi_wpa_psk_text,", config->wpaPsk);
    wifi_AtCmdWait();
  } else {
    // TODO: Maybe implement WEP.
    // (PS. I don't really want to because nobody should be using WEP anymore!)
    printf("Open and WEP wifi modes not implemented!\n");
  }

  if (config->wifiMode == 1) {
    // Configure the Wi-Fi module for 802.11n operation.
    wifi_AtCmdBlocking("AT+S.SCFG=wifi_ht_mode,1");
    // Enable all data supported rates (needed for 802.11n operation).
    wifi_AtCmdBlocking("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF");
  }

  // Change the Wi-Fi modules deafult password (anonymous) to the user provided
  // one. It can be used to configure the Wi-Fi module when the firstset.cgi is
  // enabled (although we disable it!).
  wifi_AtCmdN(2, "AT+S.SCFG=user_desc,", config->userDesc);
  wifi_AtCmdWait();

  if (config->dhcp == 1) {
    // Enable DHCP for IP.
    wifi_AtCmdBlocking("AT+S.SCFG=ip_use_dhcp,1");
  } else {
    // Disable DHCP so that we can set the user provided IPs.
    wifi_AtCmdBlocking("AT+S.SCFG=ip_use_dhcp,0");

    wifi_AtCmdN(2, "AT+S.SCFG=ip_ipaddr,", config->ipAddr);
    wifi_AtCmdWait();

    wifi_AtCmdN(2, "AT+S.SCFG=ip_netmask,", config->ipNetmask);
    wifi_AtCmdWait();

    wifi_AtCmdN(2, "AT+S.SCFG=ip_gw,", config->ipGateway);
    wifi_AtCmdWait();

    wifi_AtCmdN(2, "AT+S.SCFG=ip_dns,", config->ipDns);
    wifi_AtCmdWait();
  }

  wifi_AtCmdBlocking("AT&W"); // Write settings.
  // wifi_SoftReset();
}

// wifi_OtaUpdate performs a firmware OTA update for the WiFi module.
bool wifi_OtaUpdate(char *url) {
  WifiData *wifi = &g_wifiData;

  wifi_AtCmdN(2, "AT+S.FWUPDATE=", url);

  // We use fast processing here to quickly receive the response.
  wifi->at->status |= AT_STATUS_FAST_PROCESS;

  if (!wifi_AtCmdWait()) {
    return false;
  }

  // Issue a reset ("AT+CFUN=1") to complete the firmware update.
  wifi_SoftReset();

  wifi_WaitState(WIFI_STATE_POWER_ON);

  // Between "AT+CFUN=1" (soft reset) and power on, we should have received
  // "+WIND:17:F/W update complete!", if not the firmware update probably
  // failed.
  if ((wifi->state & WIFI_STATE_FW_UPDATE_COMPLETE) == 0) {
    return false;
  }

  wifi->state &= ~(WIFI_STATE_FW_UPDATE_COMPLETE);
  wifi_AtCmdBlocking("AT&F"); // Factory reset is mandatory after FWUPDATE.

  return true;
}

// wifi_SockdStarted returns true if the socket server is running.
bool wifi_SockdStarted(void) {
  WifiData *wifi = &g_wifiData;
  return (wifi->state & WIFI_STATE_SOCKD_STARTED) != 0;
}

// wifi_StartSockd starts the socket server, returning true if the server was
// started or is already running. Will wait (blocking) for
// WIFI_STATE_HARDWARE_STARTED.
bool wifi_StartSockd(uint16_t port) {
  WifiData *wifi = &g_wifiData;
  char sPort[5] = {0};

  if (wifi->state & WIFI_STATE_SOCKD_STARTED) {
    return true;
  }

  // Wait until WiFi hardware has started.
  while (!(wifi->state & WIFI_STATE_HARDWARE_STARTED))
    ;

  snprintf(&sPort[0], 5, "%d", port);
  wifi_AtCmdN(2, "AT+S.SOCKD=", &sPort[0]);
  if (wifi_AtCmdWait()) {
    wifi->state |= WIFI_STATE_SOCKD_STARTED;
    return true;
  } else if (strstr((char *)wifi->at->buff,
                    "Socket Server already up and running")) {
    wifi->state |= WIFI_STATE_SOCKD_STARTED;
    return true;
  }
  return false;
}

// wifi_StopSockd stops the socket server and clears related states. Returns
// true if the socket server was stopped.
bool wifi_StopSockd(void) {
  WifiData *wifi = &g_wifiData;

  while (wifi->processing)
    ;

  if ((wifi->state & WIFI_STATE_SOCKD_STARTED) == 0) {
    // Socket server is already stopped.
    return true;
  }

  printf("->wifi_StopSockd()\n");

  enterCommandMode(); // Make sure we are in command mode.

  // Socket server is stopped by setting the port to zero.
  wifi_SendString("AT+S.SOCKD=0");
  atReset(wifi->at);
  wifi->recv = RECV_AT_RESPONSE;
  wifi_Send('\r');

  if (wifi_AtCmdWait()) {
    wifi->state &= ~(WIFI_STATE_SOCKD_STARTED | WIFI_STATE_SOCKD_CLIENT_ACTIVE |
                     WIFI_STATE_SOCKD_PENDING_DATA | WIFI_STATE_DATA_MODE);
    return true;
  }
  return false;
}

uint8_t *wifi_SockdGetData(void) {
  WifiData *wifi = &g_wifiData;
  if (!(wifi->state & WIFI_STATE_SOCKD_DATA_AVAILABLE)) {
    return 0;
  }
  return wifi->sockBuff->buff;
}

void wifi_SockdClearData(void) {
  WifiData *wifi = &g_wifiData;

  clearBuffer(wifi->sockBuff);
  wifi->state &= ~(WIFI_STATE_SOCKD_DATA_AVAILABLE);
}

// wifi_SockdSend sends data to the socket client by ensuring that there is a
// client connected and entering data mode if we are in command mode.
bool wifi_SockdSendN(int n, ...) {
  va_list args;
  int i;
  WifiData *wifi = &g_wifiData;

  while (wifi->processing)
    ;

  if (!(wifi->state & WIFI_STATE_SOCKD_CLIENT_ACTIVE)) {
    return false;
  }

  enterDataMode(); // Make sure we are in data mode.

  va_start(args, n);
  for (i = 0; i < n; i++) {
    wifi_SendString(va_arg(args, const char *));
  }
  va_end(args);

  // Make sure the data is cleared (might send excess data to client).
  wifi_SendString("\r\n");

  return true;
}

// enterCommandMode leaves data mode and allows us to send AT commands to the
// WiFi module while a socket client is connected.
void enterCommandMode(void) {
  WifiData *wifi = &g_wifiData;
  int i = 0;

  while (wifi->processing)
    ;

  if (!(wifi->state & WIFI_STATE_DATA_MODE)) {
    return;
  }

  printf("->enterCommandMode()\n");

  // Send the escape sequence (default: "at+s.") as one packet (no carriage
  // return) to exit data mode.
  wifi_SendString("at+s.");

  //
  while (i++ < 10 && (wifi->state & WIFI_STATE_DATA_MODE) != 0) {
    // Toggle request (can be canceled by incoming socket data).
    wifi->reqExitDataMode = true;

    while (wifi->reqExitDataMode && (wifi->state & WIFI_STATE_DATA_MODE) != 0)
      ;

    if (!wifi->reqExitDataMode) {
      // Request canceled due to incoming socket data, keep trying...
      delay(50);
    } else {
      break;
    }
  }

  wifi_SendString("\r\n"); // Make sure "at+s." is cleared.

  wifi->reqExitDataMode = false; // Reset.

  if (i >= 10) {
    printf("enterCommandMode failed!\n");
  }
}

// enterDataMode leaves command mode and allows us to receive incoming (pending)
// data from the socket client.
void enterDataMode(void) {
  WifiData *wifi = &g_wifiData;
  int i = 0;

  while (wifi->processing)
    ;

  if (wifi->state & WIFI_STATE_DATA_MODE) {
    return;
  }

  printf("->enterDataMode()\n");

  while (i++ < 10 && !(wifi->state & WIFI_STATE_DATA_MODE)) {
    // Send escape sequence (as AT command) to enter data mode. We must send
    // this as a string because there will be no regular AT response (unless
    // there is an error).
    wifi_SendString("AT+S.");
    wifi->reqEnterDataMode = true;
    wifi_Send('\r');

    while (wifi->reqEnterDataMode && !(wifi->state & WIFI_STATE_DATA_MODE))
      ;

    if (!wifi->reqEnterDataMode) {
      printf("reqEnterDataMode denied!\n");
      delay(50);
    } else {
      break;
    }
  }

  wifi->reqEnterDataMode = false;

  if (i >= 10) {
    printf("enterDataMode failed!\n");
  }
}

struct sockState {
  uint32_t timeSince;
  bool dataReceived;
  bool httpHeader;
  bool httpBody;
};

// wifi_SysTickHandler consumes the wifi ring buffer and processes it according
// to the current wifi_RecvState. Only one char is processed per tick, except
// when AT_STATUS_FAST_PROCESS is enabled.
void wifi_SysTickHandler(void) {
  WifiData *wifi = &g_wifiData;
  uint8_t data;
  bool status;
  static struct sockState sock;

  if (sock.dataReceived) {       // Check if socket data debounce is active.
    if (sock.timeSince++ > 10) { // Debounce time elapsed?
      sock.dataReceived = false;
      sock.httpHeader = false;
      sock.httpBody = false;

      // Lock the socket buffer until it has been read and cleared by the
      // application.
      wifi->state |= WIFI_STATE_SOCKD_DATA_AVAILABLE;
    }
  }

process_loop:
  // Pop from ring buffer (thread safe).
  data = rb_Pop(wifi->ringBuff);

  wifi->processing = (data != '\0');
  if (wifi->processing) {
    switch (wifi->recv) {
    // Asynchronous indications can happen at any point except when an AT
    // command is processing, this is the default wifi recv state.
    case RECV_ASYNC_INDICATION:
      safeAppendBuffer(wifi->tmpBuff, data);
      status = checkBufferCrLf(wifi->tmpBuff);
      if (status) {
        if (processWind(&wifi->state, wifi->tmpBuff->buff)) {
          if (wifi->state & WIFI_STATE_DATA_MODE) {
            wifi->recv = RECV_SOCKD_DATA;
          } else {
            wifi->reqEnterDataMode = false;
          }
        } else {
          wifi->reqEnterDataMode = false;
          printf("Could not process WIND\n");
        }

        debugPrintBuffer(wifi->tmpBuff->buff, 'A');
        clearBuffer(wifi->tmpBuff);
      }
      break;

    // AT command responses only happen after an AT command has been issued,
    // some only return OK / ERROR whereas others have a response body, ending
    // with OK / ERROR.
    case RECV_AT_RESPONSE:
      status = processAtResponse(wifi->at, data);
      if (status) {
        wifi->recv = RECV_ASYNC_INDICATION;
      } else if ((wifi->at->status & AT_STATUS_FAST_PROCESS) != 0) {
        goto process_loop;
      }
      break;

    // Receive data from the SOCKD (socket server). Since we only support
    // receiving HTTP requests on the socket we first try to detect the HTTP
    // headers, and when we reach "\r\n\r\n" we know the rest belongs to the
    // body, so we clear the buffer at this point (discarding the header saves
    // space).
    case RECV_SOCKD_DATA:
      // If the WIFI_STATE_SOCKD_DATA_AVAILABLE state has been set, it is not
      // safe to modify the socket buffer, we must thus discard the data.
      if (!(wifi->state & WIFI_STATE_SOCKD_DATA_AVAILABLE)) {
        safeAppendBuffer(wifi->sockBuff, data); // Store data in buffer.

        if (sock.httpBody) {
          sock.dataReceived = true; // Set the data received indicator.
          sock.timeSince = 0;       // Reset "debounce" timer.
        } else if (sock.httpHeader) {
          uint8_t *buff = wifi->sockBuff->buff + wifi->sockBuff->pos - 4;

          // Check for HTTP body separator.
          if (strncmp((char *)buff, "\r\n\r\n", 4) == 0) {
            clearBuffer(wifi->sockBuff); // Remove HTTP header.
            sock.httpBody = true;        // Next up is body.
          }
        } else if (wifi->sockBuff->pos >= 15) {
          uint8_t *buff = wifi->sockBuff->buff + wifi->sockBuff->pos - 15;

          // Use Content-Length as header detection, we could additionally parse
          // the value and receive only n bytes of the body.
          if (strncmp((char *)buff, "Content-Length:", 15) == 0) {
            sock.httpHeader = true;
          }
        }
      }

      // We must check for WINDs to know if we have exited data mode. This is
      // risky since the socket client can forge WINDs to make us think the WiFi
      // module is in a different state. Unfortunately there is no way around
      // this at this time.
      safeAppendBuffer(wifi->tmpBuff, data);
      status = checkBufferCrLf(wifi->tmpBuff);
      if (status) {
        if (processWind(&wifi->state, wifi->tmpBuff->buff)) {
          if ((wifi->state & WIFI_STATE_DATA_MODE) == 0) {
            wifi->recv = RECV_ASYNC_INDICATION;
          } else {
            wifi->reqExitDataMode = false;
          }

          // Remove WIND from socket buffer, if the socket buffer position is
          // equal to or greater than the tmp buffer position, the WIND was
          // likely caught there.
          if (wifi->sockBuff->pos >= wifi->tmpBuff->pos &&
              ((wifi->state & WIFI_STATE_SOCKD_DATA_AVAILABLE) == 0)) {
            wifi->sockBuff->pos -= wifi->tmpBuff->pos; // Pre-WIND position.
            memset(wifi->sockBuff->buff + wifi->sockBuff->pos, 0,
                   WIFI_SOCK_BUFF_SIZE - wifi->sockBuff->pos);
          }
        }

        debugPrintBuffer(wifi->tmpBuff->buff, 'S');
        clearBuffer(wifi->tmpBuff); // Reset the tmp buffer.
      } else {
        wifi->reqExitDataMode = false;
      }

      // Receive the socket data as fast as possible by looping until there is
      // no more data (or we encounter a WIND that takes us out of Data Mode).
      goto process_loop;
      break;

    default:
      printf("Unknown wifi->recv state\n");
      break;
    }
  }

  // Keep looping if we must process the data as fast as possible. This likely
  // means that the ring buffer is filling up.
  if (wifi->fastProcess) {
    goto process_loop;
  }
}

// WIFI_ISR handles interrupts from the WIFI module and stores the data in a
// ring buffer.
void WIFI_ISR(void) {
  WifiData *wifi = &g_wifiData;

  // State variables for tracking (maybe) WINDs.
  static bool maybeWind = false;
  static uint8_t prevPrev = '\0';
  static uint8_t prev = '\0';

  if (usart_get_flag(WIFI_USART, USART_SR_RXNE)) {
    uint8_t data = usart_recv(WIFI_USART);

    // Keep track if we've encountered a potential "\r\n+WIND" indication with
    // minimal computation.
    if (prevPrev == '\r' && prev == '\n') {
      maybeWind = (data == '+');
    }
    // When our ring buffer is growing full, we must prioritise WINDs since they
    // are more important for knowing what state the WiFi module is in.
    if ((wifi->fastProcess = rb_HalfFull(wifi->ringBuff))) {
      // Always store "\r\n" since they are needed as line-breaks to detect
      // WINDs.
      if (data != '\r' && data != '\n' && !maybeWind) {
        prevPrev = prev;
        prev = data;
        return;
      }
    }
    rb_Push(wifi->ringBuff, data); // Push to ring buffer (thread safe).
    wifi->processing = true;       // Set flag (unprocessed data).

    // Update state.
    prevPrev = prev;
    prev = data;
  }
}

// safeAppendBuffer safely appends to a buffer by checking for buffer overflow.
static inline void safeAppendBuffer(WifiBuff *b, uint8_t data) {
  guardBufferOverflow(b);
  b->buff[b->pos++] = data;
}

// clearBuffer clears the buffer from data.
static inline void clearBuffer(WifiBuff *b) {
  memset(b->buff, 0, b->size);
  b->pos = 0;
}

// checkBufferCrLf checks a WifiBuff for a minimum length of 3, ending with
// "\r\n". By checking the length we avoid matching "\r\n" without any other
// data.
static inline bool checkBufferCrLf(WifiBuff *b) {
  return b->pos > 2 && b->buff[b->pos - 2] == '\r' &&
         b->buff[b->pos - 1] == '\n';
}

// guardBufferOverflow guards against buffer overflows by moving 1/4th of the
// buffer from the end to the middle.
static void guardBufferOverflow(WifiBuff *b) {
  if (b->pos >= b->size) {
    uint16_t sizeHalf = b->size / 2;
    uint16_t sizeFourth = b->size / 4;
    char *dest = (char *)b->buff + sizeHalf;
    char *src = (char *)b->buff + sizeHalf + sizeFourth;

    printf("Buffer overflowing, moving buffer 1/4th\n");

    // Move and cleanup the buffer.
    memmove(dest, src, sizeFourth);
    memset(src, 0, sizeFourth);

    b->pos = sizeHalf + sizeFourth;
  }
}

// processAtResponse processes the data in the provided at buffer and
// indicates whether or not the entire response has been received, the AT status
// is updated accordingly.
static bool processAtResponse(WifiAt *at, uint8_t data) {
  at->buff[at->pos++] = data;

  // AT responses always end with a "\r\n[status]\r\n".
  if (data == '\n' && at->pos > 1 && at->buff[at->pos - 2] == '\r') {
    if (at->last_cr_lf != 0) {
      // Check for AT OK response (end), indicating a successfull HTTP request.
      if (strncmp((char *)at->last_cr_lf, "\r\nOK\r\n", 6) == 0) {
        debugPrintBuffer(at->buff, '#');
        at->status = AT_STATUS_OK | AT_STATUS_READY;

        return true;
      }

      // Check for AT error response (end), indicating there was an error. We
      // check from last_cr_lf to ensure we get the full error message.
      if (strncmp((char *)at->last_cr_lf, "\r\nERROR", 7) == 0) {
        debugPrintBuffer(at->buff, '!');
        at->status = AT_STATUS_ERROR | AT_STATUS_READY;

        return true;
      }
    }

    // Keep track of the current "\r\n" position.
    at->last_cr_lf = &at->buff[at->pos - 2];
  }

  // Avoid escaping buffer constraints by moving 1/4th of the buffer from the
  // end to the middle of the buffer.
  if (at->pos >= WIFI_AT_BUFF_SIZE) {
    printf("Buffer overflowing, moving AT buffer 1/4th\n");
    char *dest = (char *)at->buff + WIFI_AT_BUFF_SIZE_HALF;
    char *src =
        (char *)at->buff + WIFI_AT_BUFF_SIZE_HALF + WIFI_AT_BUFF_SIZE_FOURTH;

    // Move and cleanup the buffer.
    memmove(dest, src, WIFI_AT_BUFF_SIZE_FOURTH);
    memset(src, 0, WIFI_AT_BUFF_SIZE_FOURTH);

    at->pos = WIFI_AT_BUFF_SIZE_HALF + WIFI_AT_BUFF_SIZE_FOURTH;

    // If last_cr_lf is part of src, it got moved and must be updated to it's
    // new position.
    // NOTE: There is a potential edge case where last_cr_lf is first moved once
    // into the middle and is then overwritten by a second move. However, in
    // practice this should not cause a problem since AT responses (OK/ERROR)
    // are not long enough.
    if ((char *)at->last_cr_lf >= src) {
      at->last_cr_lf -= WIFI_AT_BUFF_SIZE_FOURTH;
    }
  }

  return false;
}

// processWind consumes a buffer containing WIND and updates the state if a
// handled WIND ID is found.
static bool processWind(volatile WifiState *state, uint8_t *const buff) {
  WifiWind wind = WIND_UNDEFINED;
  WifiState tmpState = WIFI_STATE_OFF;
  char *windStart = NULL;

  // Must begin with either "+" or "\r\n+" to continue processing. This avoids
  // searching needlessly through large buffers for a WIND.
  if (buff[0] != '+' && buff[2] != '+') {
    return false;
  }

  windStart = strstr((char *)buff, "+WIND:");
  if (windStart) {
    windStart += 6; // Skip over "+WIND:", next char is a digit.

    // We assume the indication ID is never greater than 99 (two digits).
    wind = *windStart++ - '0'; // Convert char to int
    if (*windStart != ':') {
      wind *= 10;                 // First digit was a multiple of 10
      wind += *windStart++ - '0'; // Convert char to int
    }
    windStart++; // Skip over ':', leaving message.
  }

  switch (wind) {
  case WIND_POWER_ON:
    *state |= WIFI_STATE_POWER_ON; // Reset WIFI state after power on.
    break;

  case WIND_RESET:
    // Keep states that should not be reset.
    tmpState = (*state & WIFI_STATE_FW_UPDATE_COMPLETE);
    *state = WIFI_STATE_OFF | tmpState;
    break;

  case WIND_CONSOLE_ACTIVE:
    *state |= WIFI_STATE_CONSOLE_ACTIVE;
    break;

  case WIND_HARD_FAULT:
    // A hard fault is followed by a reboot of the WiFi module.
    *state = WIFI_STATE_OFF;
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

  case WIND_FW_UPDATE:
    // Actual message is "F/W update complete!", but we just look for part of
    // the message.
    if (strstr((char *)buff, "complete")) {
      *state |= WIFI_STATE_FW_UPDATE_COMPLETE;
    }
    break;

  case WIND_WIFI_HARDWARE_STARTED:
    *state |= WIFI_STATE_HARDWARE_STARTED;
    break;

  case WIND_COMMAND_MODE:
    *state &= ~(WIFI_STATE_DATA_MODE);

    break;
  case WIND_DATA_MODE:
    *state &= ~(WIFI_STATE_SOCKD_PENDING_DATA);
    *state |= WIFI_STATE_DATA_MODE;
    break;

  case WIND_SOCKD_CLIENT_OPEN:
    // +WIND:61:Incoming Socket Client:%i
    // %i = client IP.
    *state |= WIFI_STATE_SOCKD_CLIENT_ACTIVE;
    break;

  case WIND_SOCKD_CLIENT_CLOSE:
    *state &= ~(WIFI_STATE_SOCKD_CLIENT_ACTIVE | WIFI_STATE_SOCKD_PENDING_DATA |
                WIFI_STATE_DATA_MODE);
    break;

  case WIND_SOCKD_DROPPED_DATA:
    // +WIND:63:Sockd Dropping Data:%d:%h
    // %d = bytes dropped, %h = free heap.
    break;

  case WIND_SOCKD_PENDING_DATA:
    // +WIND:64:Sockd Pending Data:%c:%d:%e
    // %c = message count, %d = last message bytes, %e = total bytes.
    *state |= WIFI_STATE_SOCKD_PENDING_DATA;
    break;

  case WIND_UNDEFINED:
    return false;
    break;
  }

  return true;
}

// httpStatus takes a http response buffer and tries to parse the status code
// from the http header, zero is returned when no status is found.
static uint16_t httpStatus(uint8_t *response) {
  uint16_t status = 0;
  char status_str[3];
  char *header_ptr;

  header_ptr = strstr((char *)response, "HTTP/1.");
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
