#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <libopencm3/stm32/flash.h>

#include "settings.h"
#include "wifi.h"

#define DATA_EEPROM_START_ADDR 0x08080000
#define DATA_EEPROM_END_ADDR \
  0x080803FF  // We allocate 1024 bytes for settings, 4096 is max (0x08080FFF).
#define WORD_SIZE 4
#define DATA_MAX_LEN \
  (DATA_EEPROM_END_ADDR - DATA_EEPROM_START_ADDR + 1) / WORD_SIZE

static ConfigData config_data;
Config config = {false, false, &config_data};

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

void conf_Commit(Config* c) {
  eeprom_write(DATA_EEPROM_START_ADDR, c->data, sizeof(*c->data) / WORD_SIZE);
}

void conf_Load(Config* c) {
  eeprom_read(DATA_EEPROM_START_ADDR, c->data, sizeof(*c->data) / WORD_SIZE);
}

void conf_Set(Config* c, ConfigType type, void* value) {
  switch (type) {
    case conf_url1:
      memcpy(c->data->url1, (char*)value, URL_LENGTH);
      c->updated = true;
      break;
    case conf_url2:
      memcpy(c->data->url2, (char*)value, URL_LENGTH);
      c->updated = true;
      break;
    default:
      printf("Unhandled config type!\n");
  }
}

void conf_RequestCommit(Config* c) { c->commit = true; }

void conf_HandleChange(Config* c) {
  if (c->updated) {
    delay(1000);
    wifi_StoreConfigJSON(c->data);
    c->updated = false;
  }
  if (c->commit) {
    conf_Commit(c);
    c->commit = false;
  }
}
