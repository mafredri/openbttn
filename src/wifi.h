#ifndef WIFI_H
#define WIFI_H

#include "ring_buffer.h"
#include "syscfg.h"

#define WIFI_BUFF_SIZE 1024

typedef uint32_t wifi_state_t;

#define WIFI_STATE_OFF (wifi_state_t)(0 << 0)
#define WIFI_STATE_POWER_ON (wifi_state_t)(1 << 0)
#define WIFI_STATE_RESET (wifi_state_t)(2 << 0)
#define WIFI_STATE_CONSOLE_ACTIVE (wifi_state_t)(3 << 1)
#define WIFI_STATE_WIFI_UP (wifi_state_t)(4 << 2)

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

extern ring_buffer_t wifi_buffer;

#endif /* WIFI_H */
