#ifndef SPWF01SX_H
#define SPWF01SX_H

#include <ctype.h>
#include <stdint.h>

#ifndef SPWF_AT_BUFF_SIZE
#define SPWF_AT_BUFF_SIZE 1024
#endif

#define SPWF_STATE_OFF (uint16_t)(0 << 1)
#define SPWF_STATE_POWER_ON (uint16_t)(1 << 1)
#define SPWF_STATE_CONSOLE_ACTIVE (uint16_t)(2 << 1)
#define SPWF_STATE_JOINED (uint16_t)(4 << 1)
#define SPWF_STATE_ASSOCIATED (uint16_t)(8 << 1)
#define SPWF_STATE_UP (uint16_t)(16 << 1)

typedef volatile uint16_t SpwfState;
extern SpwfState spwf_State;

#define AT_STATUS_CLEAR (uint8_t)(0 << 1)
#define AT_STATUS_PENDING (uint8_t)(1 << 1)
#define AT_STATUS_FAST_PROCESS (uint8_t)(2 << 1)
#define AT_STATUS_READY (uint8_t)(4 << 1)
#define AT_STATUS_OK (uint8_t)(8 << 1)
#define AT_STATUS_ERROR (uint8_t)(16 << 1)

typedef volatile uint8_t ATStatusType;

// Define the AT state struct for dependency injection.
typedef struct ATState {
  // Current AT command status.
  ATStatusType status;
  // AT buffer status.
  uint8_t *const buff;
  uint8_t *last_cr_lf;
  uint16_t pos;
} ATState;
extern ATState spwf_ATState;

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

typedef enum {
  RECV_ASYNC_INDICATION = 0,
  RECV_AT_RESPONSE,
} RecvType;
extern RecvType spwf_RecvState;

#define SPWF_PROCESS_COMPLETE true
#define SPWF_PROCESS_INCOMPLETE false

// Must be implemented externally, used to communicate with the hardware.
extern void spwf_PowerOn(void);
extern void spwf_PowerOff(void);
extern void spwf_Send(char data);
extern void spwf_DebugSend(char data);

void spwf_Init(void);
void spwf_SoftReset(void);
void spwf_HardReset(void);
void spwf_WaitState(SpwfState state);
bool spwf_ProcessAsyncIndication(uint8_t *const buff, uint8_t data);
bool spwf_ProcessWIND(SpwfState *state, uint8_t *const buff_ptr);
bool spwf_ProcessATResponse(ATState *at, uint8_t data);
void spwf_ATCmd(char *str);
bool spwf_ATCmdBlocking(char *str);
bool spwf_ATCmdWait(void);
void spwf_SendString(const char *str);

#endif /* SPWF01SX_H */
