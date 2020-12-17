/***************************************************************************
 * ARM CPU Exception handler
 ***************************************************************************/

#if defined(__arm__) || defined(__thumb__)

#include "exception_hook.h"
#include "../backtrace/backtrace.h"
#include "../HAL_MinSerial.h"




#define HW_REG(X)  (*((volatile unsigned long *)(X)))

// Default function use the CPU VTOR register to get the vector table. 
// Accessing the CPU VTOR register is done in assembly since it's the only way that's common to all current tool
unsigned long get_vtor() 
{
    // Even if it looks like an error, it is not an error
    return HW_REG(0xE000ED08);
}
void * hook_get_hardfault_vector_address(unsigned vtor) 
{ 
    return (void*)(vtor + 0x03); 
}
void * hook_get_memfault_vector_address(unsigned vtor) 
{ 
    return (void*)(vtor + 0x04); 
}
void * hook_get_busfault_vector_address(unsigned vtor) 
{ 
    return (void*)(vtor + 0x05); 
}
void * hook_get_usagefault_vector_address(unsigned vtor) 
{ 
    return (void*)(vtor + 0x06); 
}


// From https://interrupt.memfault.com/blog/cortex-m-fault-debug with addition of saving the exception's LR and the fault type
// Please notice that the fault itself is accessible in the CFSR register at address 0xE000ED28
#define WRITE_HANDLER(X) \
  __attribute__((naked)) void FaultHandler_##X() { \
    __asm__ __volatile__ ( \
        "tst lr, #4\n" \
        "ite eq\n" \
        "mrseq r0, msp\n" \
        "mrsne r0, psp\n" \
        "mov r1,lr\n" \
        "mov r2,#" #X "\n" \
        "b CommonHandler_C\n" \
    ); \
  }

WRITE_HANDLER(1);
WRITE_HANDLER(2);
WRITE_HANDLER(3);
WRITE_HANDLER(4);

// Must be a macro to avoid creating a function frame
#define HALT_IF_DEBUGGING()                              \
  do {                                                   \
    if (HW_REG(0xE000EDF0) & (1 << 0)) { \
      __asm("bkpt 1");                                   \
    }                                                    \
} while (0)


extern "C"
void CommonHandler_C(unsigned long *sp, unsigned long lr, unsigned long cause) {

  static const char* causestr[] = {
    "Unknown","Hard","Mem","Bus","Usage",
  };

  // If you are using it'll stop here
  HALT_IF_DEBUGGING();

  // Reinit the serial link (might only work if implemented in each of your boards)
  MinSerial::init();
  
  MinSerial::TX("\n\n## Software Fault detected ##\n");
  MinSerial::TX("Cause: "); MinSerial::TX(causestr[cause]); MinSerial::TX('\n');

  MinSerial::TX("R0   : "); MinSerial::TXHex(((unsigned long)sp[0])); MinSerial::TX('\n');
  MinSerial::TX("R1   : "); MinSerial::TXHex(((unsigned long)sp[1])); MinSerial::TX('\n');
  MinSerial::TX("R2   : "); MinSerial::TXHex(((unsigned long)sp[2])); MinSerial::TX('\n');
  MinSerial::TX("R3   : "); MinSerial::TXHex(((unsigned long)sp[3])); MinSerial::TX('\n');
  MinSerial::TX("R12  : "); MinSerial::TXHex(((unsigned long)sp[4])); MinSerial::TX('\n');
  MinSerial::TX("LR   : "); MinSerial::TXHex(((unsigned long)sp[5])); MinSerial::TX('\n');
  MinSerial::TX("PC   : "); MinSerial::TXHex(((unsigned long)sp[6])); MinSerial::TX('\n');
  MinSerial::TX("PSR  : "); MinSerial::TXHex(((unsigned long)sp[7])); MinSerial::TX('\n');

  // Configurable Fault Status Register
  // Consists of MMSR, BFSR and UFSR
  MinSerial::TX("CFSR : "); MinSerial::TXHex((*((volatile unsigned long *)(0xE000ED28)))); MinSerial::TX('\n');

  // Hard Fault Status Register
  MinSerial::TX("HFSR : "); MinSerial::TXHex((*((volatile unsigned long *)(0xE000ED2C)))); MinSerial::TX('\n');

  // Debug Fault Status Register
  MinSerial::TX("DFSR : "); MinSerial::TXHex((*((volatile unsigned long *)(0xE000ED30)))); MinSerial::TX('\n');

  // Auxiliary Fault Status Register
  MinSerial::TX("AFSR : "); MinSerial::TXHex((*((volatile unsigned long *)(0xE000ED3C)))); MinSerial::TX('\n');

  // Read the Fault Address Registers. These may not contain valid values.
  // Check BFARVALID/MMARVALID to see if they are valid values
  // MemManage Fault Address Register
  MinSerial::TX("MMAR : "); MinSerial::TXHex((*((volatile unsigned long *)(0xE000ED34)))); MinSerial::TX('\n');

  // Bus Fault Address Register
  MinSerial::TX("BFAR : "); MinSerial::TXHex((*((volatile unsigned long *)(0xE000ED38)))); MinSerial::TX('\n');

  MinSerial::TX("ExcLR: "); MinSerial::TXHex(lr); MinSerial::TX('\n');
  MinSerial::TX("ExcSP: "); MinSerial::TXHex((unsigned long)sp); MinSerial::TX('\n');

  // The stack pointer is pushed by 8 words upon entering an exception, so we need to revert this
  backtrace_ex(((unsigned long)sp) + 8*4, sp[5], sp[6]); // Use the original LR not the one of the exception which we don't care if I understand correctly

  // Call the last resort function here
  hook_last_resort_func();

  // Reset now by reinstantiating the bootloader's vector table
  HW_REG(0xE000ED08) = 0;

  // Restart watchdog
  // TODO
  
  // No watchdog, let's perform ARMv7 reset instead by writing to AIRCR register with VECTKEY set to SYSRESETREQ
  HW_REG(0xE000ED0C) = (HW_REG(0xE000ED0C) & 0x0000FFFF) | 0x05FA0004;

  while(1) {} // Bad luck, nothing worked
}

void hook_cpu_exceptions()
{
    // On ARM 32bits CPU, the vector table is like this:
    // 0x0C => Hardfault
    // 0x10 => MemFault
    // 0x14 => BusFault
    // 0x18 => UsageFault
    
    // Unfortunately, it's usually run from flash, and we can't write to flash here directly to hook our instruction
    // We could set an hardware breakpoint, and hook on the fly when it's being called, but this
    // is hard to get right and would probably break debugger when attached

    // So instead, we'll allocate a new vector table filled with the previous value except
    // for the fault we are interested in.
    // Now, comes the issue to figure out what is the current vector table size
    // There is nothing telling us what is the vector table as it's per-cpu vendor specific.
    // BUT: we are being called at the end of the setup, so we assume the setup is done
    // Thus, we can read the current vector table until we find an address that's not in flash, and it would mark the
    // end of the vector table (skipping the fist entry obviously)
    // The position of the program in flash is expected to be at 0x08xxx xxxx on all known platform for ARM and the 
    // flash size is available via register 0x1FFFF7E0 on STM32 family, but it's not the case for all ARM boards 
    // (accessing this register might trigger a fault if it's not implemented).

    // So we'll simply mask the top 8 bits of the first handler as an hint of being in the flash or not -that's poor and will
    // probably break if the flash happens to be more than 128MB, but in this case, we are not magician, we need help from outside.

    unsigned long * vecAddr = (unsigned long*)get_vtor();

    #ifdef VECTOR_TABLE_SIZE
      uint32_t vec_size = VECTOR_TABLE_SIZE;
      alignas(128) static unsigned long vectable[VECTOR_TABLE_SIZE] ;
    #else
      #ifndef IS_IN_FLASH
        #define IS_IN_FLASH(X) (((unsigned long)X & 0xFF000000) == 0x08000000)
      #endif

      // When searching for the end of the vector table, this acts as a limit not to overcome
      #ifndef VECTOR_TABLE_SENTINEL 
        #define VECTOR_TABLE_SENTINEL 80
      #endif


      // Find the vector table size
      uint32_t vec_size = 1;
      while (IS_IN_FLASH(vecAddr[vec_size]) && vec_size < VECTOR_TABLE_SENTINEL) 
        vec_size++;

      // We failed to find a valid vector table size, let's abort hooking up
      if (vec_size == VECTOR_TABLE_SENTINEL) return;
      // Poor method that's wasting RAM here, but allocating with malloc and alignment would be worst
      // 128 bytes alignement is required for writing the VTOR register
      alignas(128) static unsigned long vectable[VECTOR_TABLE_SENTINEL];
    #endif
    // Copy the current vector table into the new table
    for (uint32_t i = 0; i < vec_size; i++)
      vectable[i] = vecAddr[i];

    // Let's hook now with our functions
    vectable[(unsigned long)hook_get_hardfault_vector_address(0)]  = (unsigned long)&FaultHandler_1;
    vectable[(unsigned long)hook_get_memfault_vector_address(0)]   = (unsigned long)&FaultHandler_2;
    vectable[(unsigned long)hook_get_busfault_vector_address(0)]   = (unsigned long)&FaultHandler_3;
    vectable[(unsigned long)hook_get_usagefault_vector_address(0)] = (unsigned long)&FaultHandler_4;

    // Finally swap with our own vector table
    HW_REG(0xE000ED08) = (unsigned long)vectable | (1<<29UL); // 29th bit is for telling the CPU the table is now in SRAM (should be present already)
}




#endif // __arm__ || __thumb__