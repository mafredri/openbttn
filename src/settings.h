#ifndef SETTINGS_H
#define SETTINGS_H

#include "syscfg.h"

typedef struct ConfigData {
  unsigned char url1[URL_LENGTH];
  unsigned char url2[URL_LENGTH];
} ConfigData;

typedef enum ConfigType {
  conf_url1,
  conf_url2,
} ConfigType;

typedef struct Config {
  volatile bool commit, updated;
  ConfigData* data;
} Config;
extern Config config;

void conf_Commit(Config* c);
void conf_Load(Config* c);
void conf_Set(Config* c, ConfigType type, void* value);
void conf_RequestCommit(Config* c);
void conf_HandleChange(Config* c);

#endif /* SETTINGS_H */
