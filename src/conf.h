#ifndef CONF_H
#define CONF_H

#include "syscfg.h"

#define CONF_PASSWORD_LENGTH 64
#define CONF_URL_LENGTH 120

#define CONF_PRIV_MODE_OPEN 0
#define CONF_PRIV_MODE_WEP 1
#define CONF_PRIV_MODE_WPA_WPA2 2

typedef struct ConfigData {
  char url1[CONF_URL_LENGTH + 1];
  char url2[CONF_URL_LENGTH + 1];
} __attribute__((__packed__)) ConfigData;

typedef enum ConfigType {
  CONF_URL1,
  CONF_URL2,
} ConfigType;

typedef struct Config {
  ConfigData* data;
  char password[CONF_PASSWORD_LENGTH + 1];
  volatile bool unlocked;
  volatile bool commit;
  volatile bool updated;
} Config;

void conf_Init(void);
void conf_Commit(void);
void conf_Load(void);
void conf_Unlock(const char* value);
bool conf_IsUnlocked(void);
void* conf_Get(ConfigType type);
void conf_Set(ConfigType type, const void* value);
void conf_RequestCommit(void);
void conf_CreateConfigJson(void);
void conf_HandleChange(void);

#endif /* CONF_H */
