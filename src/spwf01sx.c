#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spwf01sx.h"

// spwfState tracks the current state of the WIFI module.
SpwfState spwf_State = SPWF_STATE_OFF;

// spwf_RecvState tracks the expected response type from the WIFI module, it is
// used to decide what kind of processing is done on the response.
RecvType spwf_RecvState = RECV_ASYNC_INDICATION;

// spwf_ATState is the global state for the current AT command, state will be
// reset before executing a new AT command.
uint8_t at_buff_space[SPWF_AT_BUFF_SIZE];
ATState spwf_ATState = {AT_STATUS_CLEAR, &at_buff_space[0], 0, 0};

static void spwf_DebugPrintBuffer(uint8_t *const buff, uint8_t prefix);

// spwfDelay expects a blocking function that waits for the specified duration
// in millieseconds.
extern void spwfDelay(uint32_t ms);

// spwf_Init initialises the state of the spwf01sx module.
void spwf_Init(void) {
  spwf_State = SPWF_STATE_OFF;
  spwf_PowerOn();
}

// spwf_SoftReset executes the AT+CFUN command, issuing a reset, and waits for
// the power on indication from the WIFI module.
void spwf_SoftReset(void) {
  // We must wait for the console to be active.
  spwf_WaitState(SPWF_STATE_CONSOLE_ACTIVE);

  // Unset the power on state so that we can wait for it.
  spwf_State &= ~(SPWF_STATE_POWER_ON);

  spwf_SendString("AT+CFUN=1\r");  // Reset wifi module.
  spwf_WaitState(SPWF_STATE_POWER_ON);
}

void spwf_HardReset(void) {
  spwf_PowerOff();
  spwfDelay(1000);
  spwf_State = SPWF_STATE_OFF;
  spwf_PowerOn();
  spwf_WaitState(SPWF_STATE_POWER_ON);
}

void spwf_SendString(const char *str) {
  while (*str) {
    spwf_Send(*str++);
  }
}

// spwf_ProcessAsyncIndication any asynchronous communication from the WIFI
// module, indicating whenever a response is ready to be processed.
bool spwf_ProcessAsyncIndication(uint8_t *const buff, uint8_t data) {
  static uint16_t pos = 0;
  static uint8_t prev = '\0';

  buff[pos++] = data;

  // The beginning and the end of an asynchronous indication is marked by
  // "\r\n", by skipping the first two chars we look for pairs of "\r\n".
  if (pos > 2 && prev == '\r' && data == '\n') {
    spwf_DebugPrintBuffer(buff, '+');

    pos = 0;
    prev = '\0';
    return SPWF_PROCESS_COMPLETE;
  }

  prev = data;
  return SPWF_PROCESS_INCOMPLETE;
}

// spwf_ProcessATResponse processes the data in the provided at buffer and
// indicates whether or not the entire response has been received, the AT status
// is updated accordingly.
bool spwf_ProcessATResponse(ATState *at, uint8_t data) {
  at->buff[at->pos++] = data;

  // A response must always end at a "\r\n", by skipping len under the minimum
  // response length we avoid unecessary processing in the beginning.
  if (data == '\n' && at->buff[at->pos - 2] == '\r') {
    if (at->last_cr_lf != 0) {
      // Check for AT OK response (end), indicating a successfull HTTP request.
      if (strstr((const char *)at->last_cr_lf, "\r\nOK\r\n")) {
        spwf_DebugPrintBuffer(at->buff, '#');
        at->status = AT_STATUS_OK | AT_STATUS_READY;

        return SPWF_PROCESS_COMPLETE;
      }

      // Check for AT error response (end), indicating there was an error. We
      // check from last_cr_lf to ensure we get the full error message.
      if (strstr((const char *)at->last_cr_lf, "\r\nERROR")) {
        spwf_DebugPrintBuffer(at->buff, '!');
        at->status = AT_STATUS_ERROR | AT_STATUS_READY;

        return SPWF_PROCESS_COMPLETE;
      }
    }

    // Keep track of the current "\r\n" position.
    at->last_cr_lf = &at->buff[at->pos - 2];
  }

  return SPWF_PROCESS_INCOMPLETE;
}

// spwf_ProcessWIND consumes a buffer containing WIND and updates the state if
// a handled WIND ID is found.
bool spwf_ProcessWIND(SpwfState *state, uint8_t *const buff_ptr) {
  WINDType n = WIND_UNDEFINED;
  char *wind_ptr;

  wind_ptr = strstr((const char *)buff_ptr, "+WIND:");
  if (wind_ptr) {
    wind_ptr += 6;  // Skip over "+WIND:", next char is a digit.

    // We assume the indication ID is never greater than 99 (two digits).
    n = *wind_ptr++ - '0';  // Convert char to int
    if (*wind_ptr != ':') {
      n *= 10;               // First digit was a multiple of 10
      n += *wind_ptr - '0';  // Convert char to int
    }
  }

  switch (n) {
    case WIND_POWER_ON:
      // Reset WIFI state after power on.
      *state = SPWF_STATE_POWER_ON;
      break;
    case WIND_RESET:
      *state = SPWF_STATE_OFF;
      break;
    case WIND_CONSOLE_ACTIVE:
      *state |= SPWF_STATE_CONSOLE_ACTIVE;
      break;
    case WIND_WIFI_ASSOCIATED:
      *state |= SPWF_STATE_ASSOCIATED;
      break;
    case WIND_WIFI_JOINED:
      *state |= SPWF_STATE_JOINED;
      break;
    case WIND_WIFI_UP:
      *state |= SPWF_STATE_UP;
      break;
    case WIND_UNDEFINED:
      return false;
      break;
  }

  return true;
}

// spwf_WaitState waits until the WIFI module is in specified state.
void spwf_WaitState(SpwfState state) {
  while ((spwf_State & state) == 0) {
    ;
  }
}

// spwf_ATReset resets the AT command state.
void spwf_ATReset(ATState *at) {
  memset(at->buff, 0, at->pos);
  at->status = AT_STATUS_CLEAR;
  at->last_cr_lf = 0;
  at->pos = 0;
}

// spwf_ATCmdWait waits until we recieve the entire AT response and returns true
// if there was no error, otherwise false.
bool spwf_ATCmdWait(void) {
  while ((spwf_ATState.status & AT_STATUS_READY) == 0) {
    ;
  }

  return (spwf_ATState.status & AT_STATUS_ERROR) == 0;
}

// spwf_ATCmd sends an AT command to the WIFI module without blocking, the
// response will still be available in the AT buffer once it is received.
void spwf_ATCmd(char *str) {
  spwf_WaitState(SPWF_STATE_CONSOLE_ACTIVE);
  spwf_ATReset(&spwf_ATState);

  spwf_SendString(str);

  // Change the recv type before sending the final "\r" to prevent potential
  // race conditions. This works because sending "A" is blocking and the WIFI
  // module queues all asynchronous indications.
  spwf_RecvState = RECV_AT_RESPONSE;

  spwf_Send('\r');
}

// spwf_ATCmdBlocking sends an AT command to the WIFI module and blocks until
// it receives an OK or ERROR status. Returns true on OK and false on ERROR.
bool spwf_ATCmdBlocking(char *str) {
  spwf_ATCmd(str);
  return spwf_ATCmdWait();
}

static void spwf_DebugPrintBuffer(uint8_t *const buff, uint8_t prefix) {
  uint8_t *ptr = &buff[0];

  spwf_DebugSend(prefix);
  spwf_DebugSend('>');

  while (*ptr) {
    if (*ptr == '\r') {
      spwf_DebugSend('\\');
      spwf_DebugSend('r');
    } else if (*ptr == '\n') {
      spwf_DebugSend('\\');
      spwf_DebugSend('n');
    } else {
      spwf_DebugSend(*ptr);
    }
    ptr++;
  }

  spwf_DebugSend('\r');
  spwf_DebugSend('\n');
}
