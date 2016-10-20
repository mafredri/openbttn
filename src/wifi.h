#ifndef WIFI_H
#define WIFI_H

#include <ctype.h>

#include "conf.h"
#include "ring_buffer.h"
#include "syscfg.h"

#define WIFI_TMP_BUFF_SIZE 512  // Used for WIND (WIFI indication).
#define WIFI_AT_BUFF_SIZE 1024  // Used for AT / HTTP responses.

#define SPWF_STATE_OFF (uint16_t)(0 << 1)
#define SPWF_STATE_POWER_ON (uint16_t)(1 << 1)
#define SPWF_STATE_CONSOLE_ACTIVE (uint16_t)(2 << 1)
#define SPWF_STATE_JOINED (uint16_t)(4 << 1)
#define SPWF_STATE_ASSOCIATED (uint16_t)(8 << 1)
#define SPWF_STATE_UP (uint16_t)(16 << 1)

typedef volatile uint16_t SpwfState;

#define AT_STATUS_CLEAR (uint8_t)(0 << 1)
#define AT_STATUS_PENDING (uint8_t)(1 << 1)
#define AT_STATUS_FAST_PROCESS (uint8_t)(2 << 1)
#define AT_STATUS_READY (uint8_t)(4 << 1)
#define AT_STATUS_OK (uint8_t)(8 << 1)
#define AT_STATUS_ERROR (uint8_t)(16 << 1)

typedef volatile uint8_t ATStatusType;

// Define the AT state struct for dependency injection.
typedef struct {
  // Current AT command status.
  ATStatusType status;
  // AT buffer status.
  uint8_t *const buff;
  uint8_t *last_cr_lf;
  uint16_t pos;
} ATState;

// WIND IDs as handled by the application.
typedef enum {
  WIND_CONSOLE_ACTIVE = 0,    // Console active, can accept AT commands.
  WIND_POWER_ON = 1,          // Power on (also after reset).
  WIND_RESET = 2,             // Module will reset.
  WIND_WIFI_JOINED = 19,      // Join BSSID (AP MAC).
  WIND_WIFI_UP = 24,          // Connected with IP.
  WIND_WIFI_ASSOCIATED = 25,  // Successfull association with SSID.
  WIND_UNDEFINED = 0xFF,      // Undefined state.
} WINDType;

// Custom messages for the openbttn firmware.
typedef enum {
  CIND_COMMIT_CONFIG = 0,  // Commit configuration changes to EEPROM.
  CIND_SET_URL1 = 1,       // Set URL1 message.
  CIND_SET_URL2 = 2,       // Set URL2 message.
  CIND_UNDEF = 0xFF,       // Undefined message.
} CINDType;

typedef enum {
  RECV_ASYNC_INDICATION = 0,
  RECV_AT_RESPONSE,
} RecvType;

#define SPWF_PROCESS_COMPLETE true
#define WIFI_PROCESS_INCOMPLETE false

void wifi_SysTickHandler(void);

void wifi_Init(void);
void spwf_PowerOn(void);
void spwf_PowerOff(void);
void spwf_SoftReset(void);
void spwf_HardReset(void);
void spwf_SendString(const char *str);

void spwf_WaitState(SpwfState state);
void spwf_ATCmd(char *str);
bool spwf_ATCmdBlocking(char *str);
uint16_t wifi_http_get_request(char *url);
void wifi_get_ssid(char *dest, size_t len);

void wifi_CreateOpenBTTNPage(void);
void wifi_StoreConfigJSON(ConfigData *data);

#endif /* WIFI_H */
