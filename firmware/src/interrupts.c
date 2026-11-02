#include "py32f0xx.h"

extern volatile uint32_t system_ticks;
extern void input_tick(void);

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
  while (1) {
  }
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  system_ticks++;
  input_tick();
}
