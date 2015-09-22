/* ESP8266 drivers
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * eeprom.h: Implementation of functions for standard AT24C EEPROMS
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

#ifndef EEPROM_H
#define EEPROM_H

#include "config.h"
#include "c_types.h"
#include "osapi.h"

#define EEPROM_ADDRESS  0x51 // 0b1010001
#define EEPROM_PAGESIZE 0x20

void ICACHE_FLASH_ATTR eeprom_init(void);
bool ICACHE_FLASH_ATTR eeprom_read(const uint16_t address, uint8_t *data, const uint16_t length);
bool ICACHE_FLASH_ATTR eeprom_write(const uint16_t address, const uint8_t *data, const uint16_t length);

#endif
