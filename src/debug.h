#ifndef DEBUG_H
#define DEBUG_H

#define RCC_DEBUG_USART RCC_USART1
#define DEBUG_GPIO_PORT GPIOA
#define DEBUG_USART USART1
#define DEBUG_PIN_TX GPIO9
#define DEBUG_PIN_RX GPIO10

void debug_Init(void);
void debug_Send(char data);

#endif /* DEBUG_H */
