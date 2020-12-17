#include "HAL.h"

#if HAS_POST_MORTEM_DEBUGGING

#include "../shared/HAL_MinSerial.h"

#include "../../core/macros.h"
#include "../../core/serial.h"


#include <stdarg.h>

static void TXBegin() {
  // Disable UART interrupt in NVIC
  NVIC_DisableIRQ( UART_IRQn );

  // We NEED memory barriers to ensure Interrupts are actually disabled!
  // ( https://dzone.com/articles/nvic-disabling-interrupts-on-arm-cortex-m-and-the )
  __DSB();
  __ISB();

  // Disable clock
  pmc_disable_periph_clk( ID_UART );

  // Configure PMC
  pmc_enable_periph_clk( ID_UART );

  // Disable PDC channel
  UART->UART_PTCR = UART_PTCR_RXTDIS | UART_PTCR_TXTDIS;

  // Reset and disable receiver and transmitter
  UART->UART_CR = UART_CR_RSTRX | UART_CR_RSTTX | UART_CR_RXDIS | UART_CR_TXDIS;

  // Configure mode: 8bit, No parity, 1 bit stop
  UART->UART_MR = UART_MR_CHMODE_NORMAL | US_MR_CHRL_8_BIT | US_MR_NBSTOP_1_BIT | UART_MR_PAR_NO;

  // Configure baudrate (asynchronous, no oversampling) to BAUDRATE bauds
  UART->UART_BRGR = (SystemCoreClock / (BAUDRATE << 4));

  // Enable receiver and transmitter
  UART->UART_CR = UART_CR_RXEN | UART_CR_TXEN;
}

// A SW memory barrier, to ensure GCC does not overoptimize loops
#define sw_barrier() __asm__ volatile("": : :"memory");
static void TX(char c) {
  while (!(UART->UART_SR & UART_SR_TXRDY)) { WDT_Restart(WDT); sw_barrier(); };
  UART->UART_THR = c;
}

void install_min_serial()
{
    HAL_min_serial_init = &TXBegin;
    HAL_min_serial_out = &TX;
}

#endif