#include <stdio.h>

#include "syscfg.h"

void sys_tick_handler(void) {
  if (system_delay) {
    system_delay--;
  }
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
};
void pend_sv_handler(void) {
  while (1)
    ;
};

void BUTTON_isr(void) {
  if (exti_get_flag_status(EXTI13)) {
    exti_reset_request(EXTI13);
    if (button_pressed) {
      unsigned int ms = TIM_CNT(TIM7);
      printf("Button released: %u ms\n", ms);
      button_pressed = false;
      exti_set_trigger(EXTI13, EXTI_TRIGGER_RISING);
    } else {
      printf("Button pressed!\n");
      TIM_CNT(TIM7) = 0;
      button_pressed = true;
      exti_set_trigger(EXTI13, EXTI_TRIGGER_FALLING);
    }
  }
}
