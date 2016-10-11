#include <stdio.h>

#include "led.h"
#include "syscfg.h"
#include "wifi.h"

void sys_tick_handler(void) {
  if (system_delay) {
    system_delay--;
  }

  wifi_sys_tick_handler();
}

void nmi_handler(void) {
  while (1)
    ;
}
void hard_fault_handler(void) {
  while (1)
    ;
}
void sv_call_handler(void) {
  while (1)
    ;
}
void pend_sv_handler(void) {
  while (1)
    ;
}

void BUTTON_isr(void) {
  if (exti_get_flag_status(EXTI13)) {
    exti_reset_request(EXTI13);
    button_pressed = true;
  }
}
