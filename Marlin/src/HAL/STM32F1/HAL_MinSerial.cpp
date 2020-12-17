#include "HAL.h"

#if HAS_POST_MORTEM_DEBUGGING

#include "../shared/HAL_MinSerial.h"
#include "watchdog.h"

#include <libmaple/usart.h>
#include <libmaple/rcc.h>
#include <libmaple/nvic.h>

/* Instruction Synchronization Barrier */
#define isb() __asm__ __volatile__ ("isb" : : : "memory")

/* Data Synchronization Barrier */
#define dsb() __asm__ __volatile__ ("dsb" : : : "memory")

static void TXBegin() {
  // We use MYSERIAL0 here, so we need to figure out how to get the linked register
  struct usart_dev* dev = MYSERIAL0.c_dev();
  
  // Or use this if removing libmaple
  // int irq = dev->irq_num;
  // int nvicUART[] = { NVIC_USART1 /* = 37 */, NVIC_USART2 /* = 38 */, NVIC_USART3 /* = 39 */, NVIC_UART4 /* = 52 */, NVIC_UART5 /* = 53 */ };
  // Disabling irq means setting the bit in the NVIC ICER register located at 
  // Disable UART interrupt in NVIC
  nvic_irq_disable(dev->irq_num);

  // Use this if removing libmaple
  //NVIC_BASE->ICER[1] |= (1<< irq - 32);


  // We NEED memory barriers to ensure Interrupts are actually disabled!
  // ( https://dzone.com/articles/nvic-disabling-interrupts-on-arm-cortex-m-and-the )
  dsb();
  isb();

  rcc_clk_disable(dev->clk_id);
  rcc_clk_enable(dev->clk_id);

  usart_reg_map *regs = dev->regs;
  regs->CR1 = 0; // Reset the USART
  regs->CR2 = 0; // 1 stop bit


  // If we don't touch the BRR (baudrate register), we don't need to recompute. Else we would need to call
  usart_set_baud_rate(dev, 0, BAUDRATE);

  regs->CR1 = (USART_CR1_TE | USART_CR1_UE); // 8 bits, no parity, 1 stop bit
}

// A SW memory barrier, to ensure GCC does not overoptimize loops
#define sw_barrier() __asm__ volatile("": : :"memory");
static void TX(char c) {
  struct usart_dev* dev = MYSERIAL0.c_dev();
  while (!(dev->regs->SR & USART_SR_TXE)) {
      TERN_(USE_WATCHDOG, HAL_watchdog_refresh()); 
      sw_barrier(); 
  }
  dev->regs->DR = c;
}

void install_min_serial()
{
    HAL_min_serial_init = &TXBegin;
    HAL_min_serial_out = &TX;
}

#endif