#include "HAL.h"

#if HAS_POST_MORTEM_DEBUGGING
#include "../shared/HAL_MinSerial.h"
#include <debug_frmwrk.h>

static void TX(char c) {
  _DBC(c);
}

void install_min_serial()
{
    HAL_min_serial_out = &TX;
}

#endif
