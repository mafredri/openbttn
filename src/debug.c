#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "syscfg.h"

static void gpioSetup(void);
static void usartSetup(void);

// Implement _write for newlib(-nano)
int _write(int file, char *ptr, int len) {
  if (file == STDOUT_FILENO || file == STDERR_FILENO) {
    int i;
    for (i = 0; i < len; i++) {
      if (ptr[i] == '\n') {
        usart_send_blocking(DEBUG_USART, '\r');
      }
      usart_send_blocking(DEBUG_USART, ptr[i]);
    }
    return i;
  }
  errno = EIO;
  return -1;
}

void debug_Init(void) {
  gpioSetup();
  usartSetup();
}

void debug_Send(char data) { usart_send_blocking(DEBUG_USART, data); }

// gpioSetup configures the debug port pings in AF mode.
static void gpioSetup(void) {
  rcc_periph_clock_enable(RCC_DEBUG_USART);

  gpio_mode_setup(DEBUG_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, DEBUG_PIN_TX);
  gpio_set_output_options(DEBUG_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          DEBUG_PIN_TX);
  gpio_set_af(DEBUG_GPIO_PORT, GPIO_AF7, DEBUG_PIN_TX);

  gpio_mode_setup(DEBUG_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, DEBUG_PIN_RX);
  gpio_set_output_options(DEBUG_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                          DEBUG_PIN_RX);
  gpio_set_af(DEBUG_GPIO_PORT, GPIO_AF7, DEBUG_PIN_RX);
}

static void usartSetup(void) {
  usart_set_baudrate(DEBUG_USART, 115200);
  usart_set_databits(DEBUG_USART, 8);
  usart_set_stopbits(DEBUG_USART, USART_STOPBITS_1);
  usart_set_mode(DEBUG_USART, USART_MODE_TX);
  usart_set_parity(DEBUG_USART, USART_PARITY_NONE);
  usart_set_flow_control(DEBUG_USART, USART_FLOWCONTROL_NONE);

  usart_enable(DEBUG_USART);
}
