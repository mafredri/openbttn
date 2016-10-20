#ifndef SETTINGS_H
#define SETTINGS_H

#include "syscfg.h"

typedef struct ConfigData {
  unsigned char url1[URL_LENGTH];
  unsigned char url2[URL_LENGTH];
} ConfigData;

typedef enum ConfigType {
  CONF_URL1,
  CONF_URL2,
} ConfigType;

typedef struct Config {
  volatile bool commit;
  volatile bool updated;
  ConfigData* data;
} Config;
extern Config config;

void conf_Commit(Config* c);
void conf_Load(Config* c);
void conf_Set(Config* c, ConfigType type, const void* value);
void conf_RequestCommit(Config* c);
void conf_HandleChange(Config* c);

#endif /* SETTINGS_H */
