#ifndef WIFI_H
#define WIFI_H

#include <ctype.h>

#include "conf.h"
#include "ring_buffer.h"
#include "spwf01sx.h"
#include "syscfg.h"

#define WIFI_TMP_BUFF_SIZE 512  // Used for WIND (WIFI indication).
#define WIFI_AT_BUFF_SIZE 1024  // Used for AT / HTTP responses.

// Custom messages for the openbttn firmware.
typedef enum {
  CIND_COMMIT_CONFIG = 0,  // Commit configuration changes to EEPROM.
  CIND_SET_URL1 = 1,       // Set URL1 message.
  CIND_SET_URL2 = 2,       // Set URL2 message.
  CIND_UNDEF = 0xFF,       // Undefined message.
} CINDType;

void wifi_Init(void);
void wifi_SysTickHandler(void);
uint16_t wifi_HTTPGet(char *url);
void wifi_GetSSID(char *dest, size_t len);
void wifi_CreateOpenBTTNPage(void);
void wifi_StoreConfigJSON(ConfigData *data);

#endif /* WIFI_H */
