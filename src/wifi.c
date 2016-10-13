#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libopencmsis/core_cm3.h>
#include "wifi.h"

uint8_t wifi_rb_space[RING_BUFF_SIZE];
ring_buffer_t wifi_rb = {wifi_rb_space, 0, 0, 0};

volatile uint32_t wifi_state = WIFI_STATE_OFF;
static uint8_t tmp_buffer[1024];
static uint8_t http_buffer[WIFI_BUFF_SIZE];
volatile bool wifi_at_response_ready = false;
volatile bool wifi_at_response_ok = false;
volatile bool wifi_http_data_available = false;

typedef enum {
  wind_console_active = 0,    // Console active, can accept AT commands.
  wind_power_on = 1,          // Power on (also after reset).
  wind_reset = 2,             // Module will reset.
  wind_wifi_join = 19,        // Join BSSID (AP MAC).
  wind_wifi_up = 24,          // Connected with IP.
  wind_wifi_associated = 25,  // Successfull association with SSID.
  wind_undefined = 255,       // Unknown state.
} wifi_wind_t;

typedef enum {
  recv_async_indication = 0,
  recv_at_repsonse,
  recv_http_response,
} wifi_recv_t;

static wifi_recv_t wifi_recv_state = recv_async_indication;

void wifi_process_buffer(uint8_t data);
void wifi_process_wind(uint8_t *const buff_ptr);

static void wifi_gpio_setup(void);
static void wifi_usart_setup(void);

void wifi_init(void) {
  wifi_gpio_setup();
  wifi_usart_setup();
  wifi_on();
}

void wifi_on(void) {
  gpio_set(GPIOB, GPIO2);  // Power on Wifi module.
}

void wifi_off(void) {
  gpio_clear(GPIOB, GPIO2);  // Power off Wifi module.
}

// wifi_soft_reset executes the AT+CFUN command, issuing a reset, and waits for
// the power on indication from the WIFI module.
void wifi_soft_reset(void) {
  // Unset the power on state so that we can wait for it.
  wifi_state &= ~(WIFI_STATE_POWER_ON);

  wifi_at_command("AT+CFUN=1");  // Reset wifi module
  wifi_wait_state(WIFI_STATE_POWER_ON);
}

void wifi_hard_reset(void) {
  wifi_off();
  wifi_state = WIFI_STATE_OFF;
  delay(1000);
  wifi_on();
  wifi_wait_state(WIFI_STATE_POWER_ON);
}

static void wifi_gpio_setup(void) {
  rcc_periph_clock_enable(RCC_WIFI_USART);

  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, WIFI_GPIO_TX);
  gpio_set_output_options(WIFI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          WIFI_GPIO_TX);

  gpio_mode_setup(WIFI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, WIFI_GPIO_RX);
  gpio_set_output_options(WIFI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          WIFI_GPIO_RX);

  gpio_set_af(WIFI_GPIO_PORT, GPIO_AF7, WIFI_GPIO_TX);
  gpio_set_af(WIFI_GPIO_PORT, GPIO_AF7, WIFI_GPIO_RX);
}

static void wifi_usart_setup(void) {
  usart_set_baudrate(WIFI_USART, 115200);
  usart_set_databits(WIFI_USART, 8);
  usart_set_stopbits(WIFI_USART, USART_STOPBITS_1);
  usart_set_mode(WIFI_USART, USART_MODE_TX_RX);
  usart_set_parity(WIFI_USART, USART_PARITY_NONE);
  usart_set_flow_control(WIFI_USART, USART_FLOWCONTROL_NONE);

  nvic_enable_irq(WIFI_NVIC_IRQ);

  // Give lower priority to SYSTICK IRQ than to WIFI USART IRQ so that we can
  // keep pushing data into the ring buffer even when wifi_sys_tick_handler is
  // processing.
  nvic_set_priority(NVIC_SYSTICK_IRQ, (1 << 4));
  nvic_set_priority(WIFI_NVIC_IRQ, (0 << 4));

  usart_enable_rx_interrupt(WIFI_USART);

  usart_enable(WIFI_USART);
}

void wifi_send_string(char *str) {
  while (*str) {
    usart_send_blocking(WIFI_USART, *str++);
  }
}

// wifi_sys_tick_handler consumes the wifi ring buffer and sends it for
// processesing by the wifi_process_buffer function.
// TODO: Only process all ring buffer data when we are receiving a HTTP
// response.
void wifi_sys_tick_handler(void) {
  uint8_t data;

  __disable_irq();
  data = ring_buffer_pop(&wifi_rb);
  __enable_irq();

  while (data != '\0') {
    wifi_process_buffer(data);

    __disable_irq();
    data = ring_buffer_pop(&wifi_rb);
    __enable_irq();
  }
}

// WIFI_isr handles interrupts from the WIFI module and stores the data in a
// ring buffer.
void WIFI_isr(void) {
  uint8_t data;
  if (usart_get_flag(WIFI_USART, USART_SR_RXNE)) {
    data = usart_recv(WIFI_USART);

    __disable_irq();
    ring_buffer_push(&wifi_rb, data);
    __enable_irq();
  }
}

void wifi_process_buffer(uint8_t data) {
  static uint16_t buff_pos = 0, http_pos = 0;
  static bool http_error = false;
  static bool resp_start = false;
  static uint8_t prev_data = '\0';

  tmp_buffer[(buff_pos++) & 1023] = data;

  switch (wifi_recv_state) {
    case recv_async_indication:
      // The beginning and the end of an asynchronous indication is marked by
      // "\r\n".
      if (prev_data == '\r' && data == '\n') {
        if (resp_start) {
          usart_send_blocking(DEBUG_USART, '+');
          usart_send_blocking(DEBUG_USART, '>');
          uint16_t i = 2;
          for (; i < buff_pos; i++) {
            usart_send_blocking(DEBUG_USART, tmp_buffer[i]);
          }

          wifi_process_wind(&tmp_buffer[0]);

          buff_pos = 0;
          memset(&tmp_buffer, 0, 1024);
          resp_start = false;
        } else {
          resp_start = true;
        }
      }
      break;
    case recv_at_repsonse:
      if (prev_data == '\r' && data == '\n') {
        if (resp_start) {
          usart_send_blocking(DEBUG_USART, '=');
          usart_send_blocking(DEBUG_USART, '>');
          uint16_t i = 2;
          for (; i < buff_pos; i++) {
            usart_send_blocking(DEBUG_USART, tmp_buffer[i]);
          }

          if (strstr((const char *)&tmp_buffer, "\r\nOK\r\n")) {
            wifi_at_response_ok = true;
          } else {
            wifi_at_response_ok = false;
          }

          buff_pos = 0;
          memset(&tmp_buffer, 0, 1024);
          wifi_recv_state = recv_async_indication;
          wifi_at_response_ready = true;
          resp_start = false;
        } else {
          resp_start = true;
        }
      }
      break;
    case recv_http_response:
      // This buffer will contain both the entire HTTP response and the AT
      // command response.
      http_buffer[http_pos++] = data;

      // Do not perform unecessary parsing until enough data is received.
      if (http_pos < 6) {
        break;
      }

      bool http_response_received = false;

      if (strstr((const char *)&http_buffer[http_pos - 6], "\r\nOK\r\n")) {
        http_response_received = true;
        wifi_http_data_available = true;
      } else if (http_error && strstr((const char *)&http_buffer[http_pos - 2],
                                      "\r\n")) {  // End of error received.
        http_response_received = true;
        wifi_http_data_available = false;
      } else if (strstr((const char *)&http_buffer[http_pos - 7],
                        "\r\nERROR")) {  // Beginning of error message.

        http_error = true;
      }

      // Cleanup after complete response.
      if (http_response_received) {
        http_error = false;
        http_pos = 0;
        buff_pos = 0;
        memset(&tmp_buffer, 0, 1024);
        wifi_recv_state = recv_async_indication;
        wifi_at_response_ready = true;
      }

      break;
    default:
      printf("Unknown wifi state\n");
      break;
  }

  prev_data = data;
}

// wifi_process_wind consumes a buffer containing a WIND (WIFI indication) and
// updates the wifi_state if an appropraite WIND is found.
void wifi_process_wind(uint8_t *const buff_ptr) {
  wifi_wind_t n = wind_undefined;
  char *wind_ptr;

  wind_ptr = strstr((const char *)buff_ptr, "+WIND:");
  if (wind_ptr) {
    wind_ptr += 6;  // Skip over "+WIND:", next char is a digit.

    // We assume the WIND is never greater than 99 (two digits).
    n = *wind_ptr++ - '0';  // Convert char to int
    if (*wind_ptr != ':') {
      n *= 10;               // First digit was a multiple of 10
      n += *wind_ptr - '0';  // Convert char to int
    }
  }

  switch (n) {
    case wind_console_active:
      wifi_state |= WIFI_STATE_CONSOLE_ACTIVE;
      break;
    case wind_power_on:
      // Reset WIFI state after power on.
      wifi_state = WIFI_STATE_POWER_ON;
      break;
    case wind_reset:
      wifi_state = WIFI_STATE_RESET;
      break;
    case wind_wifi_associated:
      break;
    case wind_wifi_join:
      break;
    case wind_wifi_up:
      wifi_state |= WIFI_STATE_WIFI_UP;
      break;
    case wind_undefined:
      printf("Could not process WIND\n");
      break;
  }
}

// wifi_wait_state waits until the WIFI module is in specified state.
void wifi_wait_state(wifi_state_t state) {
  while ((wifi_state & state) == 0)
    ;
}

// wifi_at_command sends an AT command to the WIFI module without blocking, the
// result is ignored.
void wifi_at_command(char *str) {
  wifi_wait_state(WIFI_STATE_CONSOLE_ACTIVE);
  wifi_send_string(str);
  wifi_send_string("\r");
}

// wifi_at_command_blocking sends an AT command to the WIFI module and blocks
// until it receives an OK or ERROR status. Returns true on OK and false on
// error.
bool wifi_at_command_blocking(char *str) {
  wifi_wait_state(WIFI_STATE_CONSOLE_ACTIVE);
  wifi_at_response_ready = false;
  wifi_send_string(str);
  wifi_send_string("\r");
  wifi_recv_state = recv_at_repsonse;

  while (!wifi_at_response_ready)
    ;

  return wifi_at_response_ok;
}

// wifi_http_get_request performs a blocking HTTP GET request and returns the
// http status code returned by the server.
uint16_t wifi_http_get_request(char *url) {
  wifi_at_response_ready = false;
  wifi_http_data_available = false;
  memset(&http_buffer, 0, 1024);

  wifi_send_string("AT+S.HTTPGET=");
  wifi_send_string(url);
  wifi_send_string("\r");
  // Set recv state to expect a HTTP response, hopefully before WIFI module
  // gives a response.
  wifi_recv_state = recv_http_response;

  // TODO: Implement a timeout here to prevent hanging up forever.
  while (!wifi_at_response_ready)
    ;

  uint16_t http_status_code = 0;

  if (wifi_http_data_available) {
    char status_str[3];
    char *header_ptr;

    header_ptr = strstr((const char *)&http_buffer[0], "HTTP/1.");
    if (header_ptr) {
      // The status code is the 9th-11th element of the header.
      // Example: "HTTP/1.0 200 OK"
      memcpy(&status_str, (header_ptr + 9), 3);
      http_status_code = atoi(&status_str[0]);
    }
  }

  return http_status_code;
}
