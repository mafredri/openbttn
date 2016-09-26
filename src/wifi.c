#include "wifi.h"

static void wifi_gpio_setup(void);
static void wifi_usart_setup(void);

void wifi_init(void) {
  wifi_gpio_setup();
  wifi_usart_setup();
  wifi_on();
}

void wifi_on(void) {
  // Power on Wifi module.
  gpio_set(GPIOB, GPIO2);
}

void wifi_off(void) {
  // Power off Wifi module.
  gpio_clear(GPIOB, GPIO2);
}

void wifi_reset(void) {
  wifi_off();
  delay(1000);
  wifi_init();
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
  nvic_set_priority(WIFI_NVIC_IRQ, 1);

  usart_enable_rx_interrupt(WIFI_USART);

  usart_enable(WIFI_USART);
}

void wifi_send_string(char *str) {
  while (*str) {
    usart_send_blocking(WIFI_USART, *str++);
  }
}
