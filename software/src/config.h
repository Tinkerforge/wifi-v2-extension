/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * config.h: WIFI Extension 2.0 configuration
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

#ifndef CONFIG_H
#define CONFIG_H

#define UART_CONNECTION UART0
#define UART_DEBUG UART1

/*
 * WARNING:
 * Enabling DEBUG log level in mesh mode will reset the extension
 * as there will be too much debug prints. Works OK with INFO log level.
 *
 * Also note that when debugging of any level is enabled then the extension
 * will not use the configuration saved in the EEPROM but will use the default
 * configuration. Debugging is meant solely for development purpose.
 *
 * The IO2 pin of the ESP8266 module becomes UART1 TX and cannot be used as SDA
 * for the EEPROM any more. Use a Debug Brick to access the IO2 pin. On the Debug
 * Brick JTAG connector pin 5 is UART1 TX and pin 6 is GND. The baudrate is
 * 1000000 instead of the more common 115200.
 */
#define LOGGING_LEVEL LOGGING_NONE

#define FIRMWARE_VERSION_MAJOR    2
#define FIRMWARE_VERSION_MINOR    1
#define FIRMWARE_VERSION_REVISION 4

#define MESH_SINGLE_ROOT_NODE     false

#endif
