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
#define DATA_MAX_LEN                                                           \
  ((DATA_EEPROM_END_ADDR - DATA_EEPROM_START_ADDR + 1) / WORD_SIZE)

static ConfigData g_ConfigData;
Config g_Config;

static void eeprom_write(uint32_t addr, void *data, uint16_t len) {
  assert(len <= DATA_MAX_LEN);

  eeprom_program_words(addr, (uint32_t *)data, len);
}

static void eeprom_read(uint32_t addr, void *data, uint16_t len) {
  assert(len <= DATA_MAX_LEN);

  uint32_t *src = (uint32_t *)(long)addr;
  uint32_t *dst = (uint32_t *)data;
  while (len--) {
    *dst++ = *src++;
  }
}

void conf_Init(void) {
  Config *config = &g_Config;

  config->data = &g_ConfigData;
  conf_Load();
}

void conf_Save(void) {
  Config *config = &g_Config;
  eeprom_write(DATA_EEPROM_START_ADDR, config->data,
               sizeof(*config->data) / WORD_SIZE);
}

void conf_Load(void) {
  Config *config = &g_Config;
  eeprom_read(DATA_EEPROM_START_ADDR, config->data,
              sizeof(*config->data) / WORD_SIZE);
}

void *conf_Get(ConfigType type) {
  Config *config = &g_Config;

  switch (type) {
  case CONF_URL1:
    return &config->data->url1[0];
  case CONF_URL2:
    return &config->data->url2[0];
  case CONF_PASSWORD:
    return &config->data->password[0];
  default:
    printf("Unhandled config type!\n");
    break;
  }

  return 0;
}

void conf_Set(ConfigType type, const void *value) {
  Config *config = &g_Config;

  switch (type) {
  case CONF_URL1:
    memset(config->data->url1, 0, CONF_URL_LENGTH);
    strncpy(config->data->url1, (char *)value, CONF_URL_LENGTH);
    break;

  case CONF_URL2:
    memset(config->data->url2, 0, CONF_URL_LENGTH);
    strncpy(config->data->url2, (char *)value, CONF_URL_LENGTH);
    break;

  case CONF_PASSWORD:
    memset(config->data->password, 0, CONF_PASSWORD_LENGTH);
    strncpy(config->data->password, (char *)value, CONF_PASSWORD_LENGTH);
    break;

  default:
    printf("Unhandled config type!\n");
    break;
  }
}
