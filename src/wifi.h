#ifndef WIFI_H
#define WIFI_H

#include "ring_buffer.h"
#include "syscfg.h"

#define WIFI_TMP_BUFF_SIZE 512  // Used for WIND (WIFI indication).
#define WIFI_AT_BUFF_SIZE 1024  // Used for AT / HTTP responses.

typedef volatile uint16_t wifi_state_t;
typedef volatile uint8_t wifi_at_status_t;

#define WIFI_STATE_OFF (wifi_state_t)(0 << 1)
#define WIFI_STATE_POWER_ON (wifi_state_t)(1 << 1)
#define WIFI_STATE_CONSOLE_ACTIVE (wifi_state_t)(2 << 1)
#define WIFI_STATE_JOINED (wifi_state_t)(4 << 1)
#define WIFI_STATE_ASSOCIATED (wifi_state_t)(8 << 1)
#define WIFI_STATE_UP (wifi_state_t)(16 << 1)

#define AT_STATUS_CLEAR (wifi_at_status_t)(0 << 1)
#define AT_STATUS_PENDING (wifi_at_status_t)(1 << 1)
#define AT_STATUS_FAST_PROCESS (wifi_at_status_t)(2 << 1)
#define AT_STATUS_READY (wifi_at_status_t)(4 << 1)
#define AT_STATUS_OK (wifi_at_status_t)(8 << 1)
#define AT_STATUS_ERROR (wifi_at_status_t)(16 << 1)

// Define the AT state struct for dependency injection.
typedef struct {
  // Current AT command status.
  wifi_at_status_t status;
  // AT buffer status.
  uint8_t *const buff;
  uint8_t *last_cr_lf;
  uint16_t pos;
} wifi_at_t;

// WIND IDs as handled by the application.
typedef enum {
  wind_console_active = 0,    // Console active, can accept AT commands.
  wind_power_on = 1,          // Power on (also after reset).
  wind_reset = 2,             // Module will reset.
  wind_wifi_joined = 19,      // Join BSSID (AP MAC).
  wind_wifi_up = 24,          // Connected with IP.
  wind_wifi_associated = 25,  // Successfull association with SSID.
  wind_undefined = 0xFF,      // Undefined state.
} wifi_wind_t;

// Custom messages for the openbttn firmware.
typedef enum {
  bttn_set_url1 = 0,      // Set URL1 message.
  bttn_undefined = 0xFF,  // Undefined message.
} wifi_bttn_t;

typedef enum {
  recv_async_indication = 0,
  recv_at_response,
} wifi_recv_t;

#define WIFI_PROCESS_COMPLETE true
#define WIFI_PROCESS_INCOMPLETE false

void wifi_sys_tick_handler(void);

void wifi_init(void);
void wifi_on(void);
void wifi_off(void);
void wifi_soft_reset(void);
void wifi_hard_reset(void);
void wifi_send_string(char *str);

void wifi_wait_state(wifi_state_t state);
void wifi_at_command(char *str);
bool wifi_at_command_blocking(char *str);
uint16_t wifi_http_get_request(char *url);
void wifi_get_ssid(char *dest, size_t len);

extern ring_buffer_t wifi_buffer;

#endif /* WIFI_H */
