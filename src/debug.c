#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "syscfg.h"

static void debug_gpio_enable(void);
static void debug_usart_setup(void);

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

void debug_init(void) {
  debug_gpio_enable();
  debug_usart_setup();
}

static void debug_gpio_enable(void) {
  rcc_periph_clock_enable(RCC_DEBUG_USART);

  // Debug port (USART1)
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9); // PA.9 USART1_TX
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO9);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO9);

#if false
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO10);  // PA.10 USART1_RX
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO10);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO10);
#endif
}

static void debug_usart_setup(void) {
  usart_set_baudrate(DEBUG_USART, 115200);
  usart_set_databits(DEBUG_USART, 8);
  usart_set_stopbits(DEBUG_USART, USART_STOPBITS_1);
  usart_set_mode(DEBUG_USART, USART_MODE_TX);
  usart_set_parity(DEBUG_USART, USART_PARITY_NONE);
  usart_set_flow_control(DEBUG_USART, USART_FLOWCONTROL_NONE);

  usart_enable(DEBUG_USART);
}
