#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/stm32/flash.h>

#include "conf.h"
#include "util.h"
#include "wifi.h"

// We allocate 1024 bytes as configuration space in EEPROM. The maximum size is
// 4096 (end addess 0x08080FFF).
#define DATA_EEPROM_START_ADDR 0x08080000
#define DATA_EEPROM_END_ADDR 0x080803FF
#define WORD_SIZE 4
#define DATA_MAX_LEN \
  (DATA_EEPROM_END_ADDR - DATA_EEPROM_START_ADDR + 1) / WORD_SIZE

static ConfigData g_ConfigData;
Config g_Config;

static void eeprom_write(uint32_t addr, void* data, uint16_t len) {
  assert(len <= DATA_MAX_LEN);

  eeprom_program_words(addr, (uint32_t*)data, len);
}

static void eeprom_read(uint32_t addr, void* data, uint16_t len) {
  assert(len <= DATA_MAX_LEN);

  uint32_t* src = (uint32_t*)addr;
  uint32_t* dst = (uint32_t*)data;
  while (len--) {
    *dst++ = *src++;
  }
}

void conf_Init(void) {
  Config* config = &g_Config;

  config->data = &g_ConfigData;
  conf_Load();
}

void conf_Commit(void) {
  Config* config = &g_Config;
  eeprom_write(DATA_EEPROM_START_ADDR, config->data,
               sizeof(*config->data) / WORD_SIZE);
}

void conf_Load(void) {
  Config* config = &g_Config;
  eeprom_read(DATA_EEPROM_START_ADDR, config->data,
              sizeof(*config->data) / WORD_SIZE);
}

void conf_Unlock(const char* value) {
  Config* config = &g_Config;

  if (strncmp(&config->password[0], value, CONF_PASSWORD_LENGTH) == 0) {
    config->unlocked = true;
    printf("Config unlocked!\n");
  } else {
    printf("Config unlock failed, wrong password: %s\n", value);
  }
}

bool conf_IsUnlocked(void) {
  Config* config = &g_Config;
  return config->unlocked;
}

void* conf_Get(ConfigType type) {
  Config* config = &g_Config;

  switch (type) {
    case CONF_URL1:
      return &config->data->url1[0];
    case CONF_URL2:
      return &config->data->url2[0];
    default:
      printf("Unhandled config type!\n");
      break;
  }

  return 0;
}

void conf_Set(ConfigType type, const void* value) {
  Config* config = &g_Config;

  if (!config->unlocked) {
    printf("Config is locked, cannot set %d: %s\n", type, (char*)value);
    return;
  }

  switch (type) {
    case CONF_URL1:
      memset(config->data->url1, 0, CONF_URL_LENGTH);
      strncpy(config->data->url1, (char*)value, CONF_URL_LENGTH);
      config->updated = true;
      break;
    case CONF_URL2:
      memset(config->data->url2, 0, CONF_URL_LENGTH);
      strncpy(config->data->url2, (char*)value, CONF_URL_LENGTH);
      config->updated = true;
      break;
    default:
      printf("Unhandled config type!\n");
      break;
  }
}

void conf_RequestCommit(void) {
  Config* config = &g_Config;

  if (config->unlocked) {
    config->commit = true;
  } else {
    printf("Config is locked, cannot request commit.\n");
  }
}

const char configJson[] = "{\"url1\":\"%s\",\"url2\":\"%s\"}";

void conf_CreateConfigJson(void) {
  Config* config = &g_Config;
  char data[strlen(configJson) + CONF_URL_LENGTH + CONF_URL_LENGTH + 1];

  sprintf(&data[0], configJson, config->data->url1, config->data->url2);

  wifi_CreateFileInRam("config.json", "application/json", &data[0],
                       strlen(data));
}

// conf_HandleChange handles changes to the config and must be run periodically
// to make sure changes are persisted.
// TODO: Prevent blocking by checking system ticks and using timestamps instead
// of the current deboucne method.
void conf_HandleChange(void) {
  Config* config = &g_Config;

  if (config->updated) {
    config->updated = false;
    delay(500);              // Debounce time.
    if (!config->updated) {  // No update in 500ms.
      conf_CreateConfigJson();
      printf("Config updated!\n");
    } else {
      return;  // New update, abort.
    }
  }
  if (config->commit) {
    config->commit = false;
    delay(2000);            // Long debounce for commits.
    if (!config->commit) {  // No commit in 2s.
      conf_Commit();
      printf("Config committed!\n");
    }
  }

  // The configuration should not remain unlocked indefinitely, unless a new
  // update is received ,
  if (config->unlocked) {
    delay(2000);
    if (!config->updated) {
      config->unlocked = false;
      printf("Config locked!\n");
    }
  }
}
