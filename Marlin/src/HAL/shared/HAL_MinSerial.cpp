#include "HAL_MinSerial.h"
#include "../../core/serial.h"

void HAL_min_serial_init_default() {}
void HAL_min_serial_out_default(char ch) { SERIAL_CHAR(ch); }
void (*HAL_min_serial_init)() = &HAL_min_serial_init_default;
void (*HAL_min_serial_out)(char) = &HAL_min_serial_out_default;
