#ifndef WIFI_H
#define WIFI_H

#include "ring_buffer.h"
#include "syscfg.h"

#define WIFI_BUFF_SIZE 1024

void wifi_sys_tick_handler(void);

void wifi_init(void);
void wifi_on(void);
void wifi_off(void);
void wifi_reset(void);
void wifi_send_string(char *str);

uint16_t wifi_http_get_request(char *url);

extern ring_buffer_t wifi_buffer;

#endif /* WIFI_H */
