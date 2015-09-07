/* ESP8266
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * debug.c: ESP8266 debug functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "debug.h"

#include "uart.h"

void ICACHE_FLASH_ATTR debug_enable(const uint8_t uart) {
	uart_configure(uart, 115200, UART_PARITY_NONE, UART_STOP_ONE, UART_DATA_EIGHT);
	uart_set_printf(uart);
	uart_enable(uart);
}
