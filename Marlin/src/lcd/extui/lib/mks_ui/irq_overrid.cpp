/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "../../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI

#include "draw_ui.h"

#if ENABLED(USE_WIFI_FUNCTION)

#include "wifiSerial.h"

#include <libmaple/libmaple.h>
#include <libmaple/gpio.h>
#include <libmaple/timer.h>
#include <libmaple/usart.h>
#include <libmaple/ring_buffer.h>

#include "../../../../inc/MarlinConfig.h"

#ifdef __cplusplus
  extern "C" { /* C-declarations for C++ */
#endif

#define WIFI_IO1_SET()    WRITE(WIFI_IO1_PIN, HIGH);
#define WIFI_IO1_RESET()  WRITE(WIFI_IO1_PIN, LOW);

void __irq_usart1(void) {
   if ((USART1_BASE->CR1 & USART_CR1_RXNEIE) && (USART1_BASE->SR & USART_SR_RXNE)) {
   	WRITE(WIFI_IO1_PIN, HIGH);
   }
   WIFISERIAL.wifi_usart_irq(USART1_BASE);
}

#ifdef __cplusplus
  } /* C-declarations for C++ */
#endif

#endif // USE_WIFI_FUNCTION
#endif // HAS_TFT_LVGL_UI
