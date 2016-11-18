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

#define LOGGING_LEVEL LOGGING_NONE

/*
 * This is a temporary define. Ultimately this define will be replaced with a
 * configuration a field to determine whether the extension is in mesh mode or
 * not. Actual application of mesh settings will be implemented in the function
 * configuration_apply().
 *
 * To enable, set it to 1, any other value to diable.
 *
 * Note that if mesh experimental code is enabled it will require some libraries
 * in the SDK to be updated otherwise compilation will fail. To achieve this
 * uncomment the rule "all" in the Makefile.
 *
 * By default mesh mode is disabled.
 */
#define MESH_ENABLED 0

#define FIRMWARE_VERSION_MAJOR    2
#define FIRMWARE_VERSION_MINOR    0
#define FIRMWARE_VERSION_REVISION 3

#endif
