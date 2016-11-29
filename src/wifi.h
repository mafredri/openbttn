#ifndef WIFI_H
#define WIFI_H

#include <stdarg.h>

#include "conf.h"
#include "ring_buffer.h"
#include "syscfg.h"

#define RCC_WIFI_USART RCC_USART3
#define WIFI_GPIO_PORT GPIOB
#define WIFI_USART USART3
#define WIFI_GPIO_TX GPIO10
#define WIFI_GPIO_RX GPIO11
#define WIFI_GPIO_CTS GPIO13
#define WIFI_GPIO_RTS GPIO14
#define WIFI_ISR usart3_isr
#define WIFI_NVIC_IRQ NVIC_USART3_IRQ

#ifndef WIFI_TMP_BUFF_SIZE
#define WIFI_TMP_BUFF_SIZE 512 // Used for WIND (WIFI indication).
#endif
#ifndef WIFI_AT_BUFF_SIZE
#define WIFI_AT_BUFF_SIZE 1024 // Used for AT / HTTP responses.
#endif
#define WIFI_AT_BUFF_SIZE_HALF (WIFI_AT_BUFF_SIZE / 2)
#define WIFI_AT_BUFF_SIZE_FOURTH (WIFI_AT_BUFF_SIZE_HALF / 2)
#ifndef WIFI_SOCK_BUFF_SIZE
#define WIFI_SOCK_BUFF_SIZE 1024 // Used for socket requests.
#endif

#define WIFI_STATE_OFF (uint16_t)(0)
#define WIFI_STATE_POWER_ON (uint16_t)(1 << 0)
#define WIFI_STATE_CONSOLE_ACTIVE (uint16_t)(1 << 1)
#define WIFI_STATE_JOINED (uint16_t)(1 << 2)
#define WIFI_STATE_ASSOCIATED (uint16_t)(1 << 3)
#define WIFI_STATE_UP (uint16_t)(1 << 4)
#define WIFI_STATE_FW_UPDATE_COMPLETE (uint16_t)(1 << 5)
#define WIFI_STATE_DATA_MODE (uint16_t)(1 << 6)
#define WIFI_STATE_COMMAND_MODE_REQUESTED (uint16_t)(1 << 7)
#define WIFI_STATE_SOCKD_STARTED (uint16_t)(1 << 8)
#define WIFI_STATE_SOCKD_CLIENT_ACTIVE (uint16_t)(1 << 9)
#define WIFI_STATE_SOCKD_SAFE_CLIENT_ACTIVE (uint16_t)(1 << 10)
#define WIFI_STATE_SOCKD_PENDING_DATA (uint16_t)(1 << 11)
#define WIFI_STATE_SOCKD_DATA_AVAILABLE (uint16_t)(1 << 12)
#define WIFI_STATE_HARDWARE_STARTED (uint16_t)(1 << 13)

#define AT_STATUS_CLEAR (uint8_t)(0)
#define AT_STATUS_PENDING (uint8_t)(1 << 1)
#define AT_STATUS_FAST_PROCESS (uint8_t)(1 << 2)
#define AT_STATUS_READY (uint8_t)(1 << 3)
#define AT_STATUS_OK (uint8_t)(1 << 4)
#define AT_STATUS_ERROR (uint8_t)(1 << 5)

#define WIFI_CONFIG_USER_DESC_LENGTH CONF_PASSWORD_LENGTH
#if WIFI_CONFIG_USER_DESC_LENGTH > 64
#error "Max length of WIFI_CONFIG_USER_DESC_LENGTH is 64"
#endif
#define WIFI_CONFIG_SSID_LENGTH 32
#define WIFI_CONFIG_WPA_PSK_LENGTH 64
#define WIFI_FILE_MAX_SIZE 4096
#define WIFI_URL_LENGTH 200

#define HTTP_HEADER_LENGTH 140
#define HTTP_HEADER_ENCODING_LENGTH                                            \
  (18 + 8) // Longest encodings are "compress" and "identity" (8 chars).

typedef uint16_t WifiState;

typedef enum WifiRecv WifiRecv;
enum WifiRecv {
  RECV_ASYNC_INDICATION = 0,
  RECV_AT_RESPONSE,
  RECV_SOCKD_DATA,
};

typedef struct WifiAt WifiAt;
struct WifiAt {
  volatile uint8_t status; // Current AT command status.
  uint8_t *const buff;     // AT buffer status.
  uint8_t *last_cr_lf;
  uint16_t pos;
};

typedef struct WifiConfig WifiConfig;
struct WifiConfig {
  char otaUrl[WIFI_URL_LENGTH + 1];
  char userDesc[WIFI_CONFIG_USER_DESC_LENGTH + 1];
  char ssid[WIFI_CONFIG_SSID_LENGTH + 1];
  char wpaPsk[WIFI_CONFIG_WPA_PSK_LENGTH + 1];
  uint8_t privMode;
  uint8_t wifiMode;
  uint8_t dhcp;
  char ipAddr[15 + 1];
  char ipNetmask[15 + 1];
  char ipGateway[15 + 1];
  char ipDns[15 + 1];
  bool commit;
  bool otaPending;
};

typedef struct WifiBuff WifiBuff;
struct WifiBuff {
  uint8_t *const buff;
  uint16_t const size;
  volatile uint16_t pos;
};

typedef struct WifiData WifiData;
struct WifiData {
  volatile WifiState state;
  volatile WifiRecv recv;
  RingBuffer *ringBuff;
  WifiAt *at;
  WifiBuff *tmpBuff;
  WifiBuff *sockBuff;
  volatile bool processing;
  volatile bool fastProcess;
  volatile bool reqEnterDataMode;
  volatile bool reqExitDataMode;
};

// WIND IDs as handled by the application.
typedef enum WifiWind WifiWind;
enum WifiWind {
  WIND_CONSOLE_ACTIVE = 0, // Console active, can accept AT commands.
  WIND_POWER_ON = 1,       // Power on (also after reset).
  WIND_RESET = 2,          // Module will reset.
  // Hard fault can happen e.g. when a large enough payload is sent to the
  // socket server and the client disconnects before the data has been
  // transmitted over the UART. Example WIND:
  // +WIND:8:Hard Fault:CW1200RxPrs: r0 00000070, r1 00000078, r2 00000068, r3
  // B3E51B1F, r12 00000002, lr 08016365, pc 080163A4, psr 21000000
  WIND_HARD_FAULT = 8,       // OS hard fault detected.
  WIND_FW_UPDATE = 17,       // Firmware update in progress (status).
  WIND_WIFI_JOINED = 19,     // Join BSSID (AP MAC).
  WIND_WIFI_UP = 24,         // Connected with IP.
  WIND_WIFI_ASSOCIATED = 25, // Successfull association with SSID.
  WIND_WIFI_HARDWARE_STARTED =
      32, // Radio reports successful internal initialization.
  WIND_COMMAND_MODE = 59,
  WIND_DATA_MODE = 60,

  WIND_SOCKD_CLIENT_OPEN = 61,
  WIND_SOCKD_CLIENT_CLOSE = 62,
  // +WIND:63:Sockd Dropping Data:<bd>:<fh>
  // bd = Bytes dropped
  // fh = Free heap
  WIND_SOCKD_DROPPED_DATA = 63,
  // +WIND:64:Sockd Pending Data:<nm>:<nb>:<tb>
  // nm = Number of messages
  // nb = Number of bytes in last message
  // tb = Total bytes received
  WIND_SOCKD_PENDING_DATA = 64,

  WIND_UNDEFINED = 0xFF, // Undefined state.
};

extern WifiData g_wifiData;

void wifi_Init(void);
void wifi_PowerOn(void);
void wifi_PowerOff(void);
void wifi_SoftReset(void);
void wifi_HardReset(void);
void wifi_WaitState(WifiState states);
void wifi_AtCmdN(int n, ...);
void wifi_AtCmd(char *str);
bool wifi_AtCmdBlocking(char *str);
bool wifi_AtCmdWait(void);
void wifi_Send(char data);
void wifi_SendString(const char *str);
uint16_t wifi_HttpGet(char *url);
void wifi_EnableFirstConfig(const char *ssid);
int wifi_CreateHttpHeader(char *dest, int len, int status,
                          const char *statusText, const char *contentType,
                          const char *contentEnc, uint16_t contentLength);
void wifi_CreateFileInRam(const char *name, const char *header,
                          const char *data, uint16_t dataSize);
void wifi_ApplyConfig(WifiConfig *config);
bool wifi_OtaUpdate(char *url);
bool wifi_SockdStarted(void);
void wifi_SockdHandler(void);
void wifi_SockdIsSafeClient(void);
bool wifi_StartSockd(uint16_t port);
bool wifi_StopSockd(void);
uint8_t *wifi_SockdGetData(void);
void wifi_SockdClearData(void);
bool wifi_SockdSendN(int n, ...);
void wifi_SysTickHandler(void);

#endif /* WIFI_H */
