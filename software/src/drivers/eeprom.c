/* ESP8266 drivers
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * eeprom.c: Implementation of functions for standard AT24C EEPROMS
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

#include "eeprom.h"

#include "config.h"
#include "c_types.h"
#include "osapi.h"
#include "i2c_master.h"
#include "utility.h"
#include "gpio.h"
#include "user_interface.h"

bool ICACHE_FLASH_ATTR eeprom_read_page(const uint16_t address, uint8_t *data, const uint16_t length) {
	i2c_master_start();
	i2c_master_send((EEPROM_ADDRESS << 1) | I2C_MASTER_WRITE);
	i2c_master_send(address >> 8);
	i2c_master_send(address & 0xFF);
	i2c_master_stop();

	i2c_master_start();
	i2c_master_send((EEPROM_ADDRESS << 1) | I2C_MASTER_READ);
	for(uint8_t i = 0; i < length; i++) {
		data[i] = i2c_master_recv(i != (length - 1));
	}
	i2c_master_stop();

    return true;
}

bool ICACHE_FLASH_ATTR eeprom_write_page(const uint16_t address, const uint8_t *data, const uint16_t length) {
	if(((address % EEPROM_PAGESIZE) + length) > EEPROM_PAGESIZE) {
		// The length does not fit in one page (starting from address)
		return false;
	}

	bool ret = true;

	i2c_master_start();
	i2c_master_send((EEPROM_ADDRESS << 1) | I2C_MASTER_WRITE);
	i2c_master_send(address >> 8);
	i2c_master_send(address & 0xFF);
	for(uint8_t i = 0; i < length; i++) {
		if(!i2c_master_send(data[i])) {
			ret = false;
			break;
		}
	}
	i2c_master_stop();

	// TODO feed watchdog here after we update to newer sdk
//	system_soft_wdt_feed();
	os_delay_us(10*1000);
//	system_soft_wdt_feed();

    return ret;
}

void ICACHE_FLASH_ATTR eeprom_select(void) {
	gpio_output_set(BIT5, 0, BIT5, 0);
	os_delay_us(100);
}

void ICACHE_FLASH_ATTR eeprom_deselect(void) {
	os_delay_us(100);
	gpio_output_set(0, BIT5, BIT5, 0);
}

void ICACHE_FLASH_ATTR eeprom_init(void) {
    //Set GPIO5 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

    eeprom_deselect();
}

bool ICACHE_FLASH_ATTR eeprom_read(const uint16_t address, uint8_t *data, const uint16_t length) {
	eeprom_select();

	uint8_t reminder = EEPROM_PAGESIZE - (address % EEPROM_PAGESIZE);
	if(reminder != EEPROM_PAGESIZE) {
		reminder = MIN(length, reminder);
		if(!eeprom_read_page(address, data, reminder)) {
			eeprom_deselect();
			return false;
		}
	} else {
		reminder = 0;
	}

	uint8_t i = 0;
	for(i = 0; i < (length - reminder)/EEPROM_PAGESIZE; i++) {
		if(!eeprom_read_page(address + reminder + i*EEPROM_PAGESIZE, data + reminder + i*EEPROM_PAGESIZE, EEPROM_PAGESIZE)) {
			eeprom_deselect();
			return false;
		}
	}

	uint8_t last = (length - reminder) - i*EEPROM_PAGESIZE;
	if(last != 0) {
		if(!eeprom_read_page(address + reminder + i*EEPROM_PAGESIZE, data + reminder + i*EEPROM_PAGESIZE, last)) {
			eeprom_deselect();
			return false;
		}
	}

	eeprom_deselect();
    return true;
}


bool ICACHE_FLASH_ATTR eeprom_write(const uint16_t address, const uint8_t *data, const uint16_t length) {
	eeprom_select();

	uint8_t reminder = EEPROM_PAGESIZE - (address % EEPROM_PAGESIZE);
	if(reminder != EEPROM_PAGESIZE) {
		reminder = MIN(length, reminder);
		if(!eeprom_write_page(address, data, reminder)) {
			eeprom_deselect();
			return false;
		}
	} else {
		reminder = 0;
	}

	uint8_t i = 0;
	for(i = 0; i < (length - reminder)/EEPROM_PAGESIZE; i++) {
		if(!eeprom_write_page(address + reminder + i*EEPROM_PAGESIZE, data + reminder + i*EEPROM_PAGESIZE, EEPROM_PAGESIZE)) {
			eeprom_deselect();
			return false;
		}
	}

	uint8_t last = (length - reminder) - i*EEPROM_PAGESIZE;
	if(last != 0) {
		if(!eeprom_write_page(address + reminder + i*EEPROM_PAGESIZE, data + reminder + i*EEPROM_PAGESIZE, last)) {
			eeprom_deselect();
			return false;
		}
	}

	eeprom_deselect();
    return true;
}
