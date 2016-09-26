#ifndef WIFI_H
#define WIFI_H

#include "syscfg.h"

void wifi_init(void);
void wifi_on(void);
void wifi_off(void);
void wifi_reset(void);
void wifi_send_string(char *str);

#endif /* WIFI_H */
