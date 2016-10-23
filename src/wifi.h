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
#define WIFI_TMP_BUFF_SIZE 1024  // Used for WIND (WIFI indication).
#endif
#ifndef WIFI_AT_BUFF_SIZE
#define WIFI_AT_BUFF_SIZE (1024 * 4)  // Used for AT / HTTP responses.
#endif

#define WIFI_STATE_OFF (uint16_t)(0)
#define WIFI_STATE_POWER_ON (uint16_t)(1 << 1)
#define WIFI_STATE_CONSOLE_ACTIVE (uint16_t)(1 << 2)
#define WIFI_STATE_JOINED (uint16_t)(1 << 3)
#define WIFI_STATE_ASSOCIATED (uint16_t)(1 << 4)
#define WIFI_STATE_UP (uint16_t)(1 << 5)

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
#define WIFI_CONFIG_PASSWORD_LENGTH 64

#define WIFI_FILE_MAX_SIZE 4096

#define WIFI_PROCESS_COMPLETE true
#define WIFI_PROCESS_INCOMPLETE false

typedef volatile uint16_t WifiStateType;

typedef volatile enum WifiRecvType {
  RECV_ASYNC_INDICATION = 0,
  RECV_AT_RESPONSE,
} WifiRecvType;

typedef struct WifiAtType {
  volatile uint8_t status;  // Current AT command status.
  uint8_t *const buff;      // AT buffer status.
  uint8_t *last_cr_lf;
  uint16_t pos;
} WifiAtType;

typedef struct WifiConfigType {
  char userDesc[WIFI_CONFIG_USER_DESC_LENGTH + 1];
  char ssid[WIFI_CONFIG_SSID_LENGTH + 1];
  char password[WIFI_CONFIG_PASSWORD_LENGTH + 1];
  uint8_t privMode;
  uint8_t dhcp;
  uint32_t ipAddr;
  uint32_t ipNetmask;
  uint32_t ipGateway;
  uint32_t ipDns;
  bool changed;
  bool authenticated;
} WifiConfigType;

typedef struct WifiDataType {
  WifiStateType state;
  WifiRecvType recv;
  WifiAtType *at;
  WifiConfigType *config;
  RingBufferType *ringBuff;
  uint8_t *tmpBuff;
} WifiDataType;
extern WifiDataType g_WifiData;

// WIND IDs as handled by the application.
typedef enum {
  WIND_CONSOLE_ACTIVE = 0,    // Console active, can accept AT commands.
  WIND_POWER_ON = 1,          // Power on (also after reset).
  WIND_RESET = 2,             // Module will reset.
  WIND_WIFI_JOINED = 19,      // Join BSSID (AP MAC).
  WIND_WIFI_UP = 24,          // Connected with IP.
  WIND_WIFI_ASSOCIATED = 25,  // Successfull association with SSID.
  WIND_UNDEFINED = 0xFF,      // Undefined state.
} WifiWindType;

#define WIFI_CIND_TEXT_LENGTH 120
#define WIFI_CIND_MESSAGE_LENGTH (WIFI_CIND_TEXT_LENGTH - 9)  // +CIND:00:

// Custom messages for the openbttn firmware.
typedef enum {
  CIND_AUTHENTICATE = 0,
  CIND_COMMIT_CONFIG = 1,  // Commit configuration changes to EEPROM.
  CIND_SET_URL1 = 2,       // Set URL1 message.
  CIND_SET_URL2 = 3,       // Set URL2 message.

  // Configuration for the SPWF01SA WIFI module.
  CIND_SET_USER_DESC = 20,  // Set system authentication password.
  CIND_SET_SSID = 21,
  CIND_SET_PASSWORD = 22,
  CIND_SET_PRIV_MODE = 23,
  CIND_SET_DHCP = 24,
  CIND_SET_IP_ADDR = 25,
  CIND_SET_IP_NETMASK = 26,
  CIND_SET_IP_GATEWAY = 27,
  CIND_SET_IP_DNS = 28,
  CIND_WIFI_COMMIT = 29,
  CIND_UNDEF = 0xFF,  // Undefined message.
} WifiCindType;

void wifi_Init(void);
void wifi_PowerOn(void);
void wifi_PowerOff(void);
void wifi_SoftReset(void);
void wifi_HardReset(void);
void wifi_WaitState(WifiStateType states);
void wifi_HandleChange(void);
void wifi_AtCmdN(int n, ...);
void wifi_AtCmd(char *str);
bool wifi_AtCmdBlocking(char *str);
bool wifi_AtCmdWait(void);
void wifi_Send(char data);
void wifi_SendString(const char *str);
uint16_t wifi_HttpGet(char *url);
void wifi_GetSsid(char *dest, size_t len);
void wifi_EnableFirstConfig(void);
void wifi_CreateFileInRam(const char *name, const char *contentType,
                          const char *data, uint16_t contentLength);
void wifi_ApplyConfig(void);
void wifi_SysTickHandler(void);

#endif /* WIFI_H */
