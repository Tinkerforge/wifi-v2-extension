/* ESP8266 drivers
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * i2c_master.c: Implementation of i2c master
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

#include "i2c_master.h"

#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

// We force GCC to inline all of the things. With this we can be sure
// that the timings stay the same, even if we change the optimization
// level or GCC changes optimizations in a new version.
// With the current code we can generate a decent 400kHz clock
// (with data changes) without big timing differences.

static inline __attribute__((always_inline)) bool i2c_scl_value(void) {
	return GPIO_INPUT_GET(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO));
}

static inline __attribute__((always_inline)) void i2c_scl_high(void) {
	gpio_output_set(1 << I2C_MASTER_SCL_GPIO, 0, 1 << I2C_MASTER_SCL_GPIO, 0);
	uint8_t i = 255;
	while(!i2c_scl_value() && --i != 0); // allow slave to clock-stretch
}

static inline __attribute__((always_inline)) void i2c_scl_low(void) {
	gpio_output_set(0, 1 << I2C_MASTER_SCL_GPIO, 1 << I2C_MASTER_SCL_GPIO, 0);
}

static inline __attribute__((always_inline)) bool i2c_sda_value(void) {
	return GPIO_INPUT_GET(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO));
}

static inline __attribute__((always_inline)) void i2c_sda_high(void) {
	gpio_output_set(1 << I2C_MASTER_SDA_GPIO, 0, 1 << I2C_MASTER_SDA_GPIO, 0);
}

static inline __attribute__((always_inline)) void i2c_sda_low(void) {
	gpio_output_set(0, 1 << I2C_MASTER_SDA_GPIO, 1 << I2C_MASTER_SDA_GPIO, 0);
}

static inline __attribute__((always_inline)) void i2c_scl_high_sda_low(void) {
	gpio_output_set(1 << I2C_MASTER_SCL_GPIO, 1 << I2C_MASTER_SDA_GPIO, 1 << I2C_MASTER_SDA_GPIO | 1 << I2C_MASTER_SCL_GPIO, 0);
}

static inline __attribute__((always_inline)) void i2c_scl_high_sda_high(void) {
	gpio_output_set(1 << I2C_MASTER_SDA_GPIO | 1 << I2C_MASTER_SCL_GPIO, 0, 1 << I2C_MASTER_SDA_GPIO | 1 << I2C_MASTER_SCL_GPIO, 0);
}

static inline __attribute__((always_inline)) void i2c_scl_low_sda_low(void) {
	gpio_output_set(0, 1 << I2C_MASTER_SDA_GPIO | 1 << I2C_MASTER_SCL_GPIO, 1 << I2C_MASTER_SDA_GPIO | 1 << I2C_MASTER_SCL_GPIO, 0);
}

static inline __attribute__((always_inline)) void i2c_scl_low_sda_high(void) {
	gpio_output_set(1 << I2C_MASTER_SDA_GPIO, 1 << I2C_MASTER_SCL_GPIO, 1 << I2C_MASTER_SDA_GPIO | 1 << I2C_MASTER_SCL_GPIO, 0);
}

static inline __attribute__((always_inline)) void i2c_sleep_halfclock(void) {
	// Half a clock cycle for 400kHz (found per trial and error)
	for(volatile uint8_t i = 0; i < 4; i++) {
		// Nothing
	}
}

static inline __attribute__((always_inline)) void i2c_sleep_quarterclock(void) {
	// Quarter of a clock cycle for 400kHz (found per trial and error)
	for(volatile uint8_t i = 0; i < 4; i++) {
		// Nothing
	}
}

void inline __attribute__((always_inline)) i2c_master_stop(void) {
	i2c_scl_low_sda_low();
	i2c_sleep_halfclock();
	i2c_scl_high();
	i2c_sleep_halfclock();
	i2c_sda_high();
	i2c_sleep_halfclock();
}

void inline __attribute__((always_inline)) i2c_master_start(void) {
	i2c_scl_high();
	i2c_sleep_halfclock();
	i2c_sda_low();
	i2c_sleep_halfclock();
}

uint8_t inline __attribute__((always_inline)) i2c_master_recv(bool ack) {
	uint8_t value = 0;

	for(int8_t i = 7; i >= 0; i--) {
		i2c_scl_low_sda_high();
		i2c_sleep_halfclock();
		i2c_scl_high();
		if(i2c_sda_value()) {
			value |= (1 << i);
		}
		// The if above takes about 1/4 of the I2C clock cycle
		i2c_sleep_quarterclock();
	}

	// ACK
	if(ack) {
		i2c_scl_low_sda_low();
	} else {
		i2c_scl_low_sda_high();
	}
	i2c_sleep_halfclock();
	i2c_scl_high();
	i2c_sleep_halfclock();

	return value;
}

bool inline __attribute__((always_inline)) i2c_master_send(const uint8_t value) {
	for(int8_t i = 7; i >= 0; i--) {
		if((value >> i) & 1) {
			i2c_scl_low_sda_high();
		} else {
			i2c_scl_low_sda_low();
		}
		i2c_sleep_halfclock();
		i2c_scl_high();
		// The if above takes about 1/4 of the I2C clock cycle
		i2c_sleep_quarterclock();
	}

	i2c_scl_low_sda_high(); // Make sure SDA is always released

	// Wait for ACK
	bool ret = false;

	i2c_sleep_halfclock();
	i2c_scl_high();
	if(!i2c_sda_value()) {
		ret = true;
	}

	// The if above takes about 1/4 of the I2C clock cycle
	i2c_sleep_quarterclock();

	return ret;
}

void ICACHE_FLASH_ATTR i2c_master_init(void) {
    ETS_GPIO_INTR_DISABLE() ;

    i2c_scl_high_sda_high();

    PIN_FUNC_SELECT(I2C_MASTER_SDA_MUX, I2C_MASTER_SDA_FUNC);
    PIN_FUNC_SELECT(I2C_MASTER_SCL_MUX, I2C_MASTER_SCL_FUNC);

    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SDA_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_MASTER_SDA_GPIO));
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(I2C_MASTER_SCL_GPIO))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << I2C_MASTER_SCL_GPIO));

    ETS_GPIO_INTR_ENABLE();

    os_delay_us(10);

    // Clear I2C bus
    i2c_sda_low();
    os_delay_us(10);
    i2c_scl_low();
    os_delay_us(10);

    for(uint8_t i = 0; i < 9; i++) {
    	i2c_scl_low();
    	os_delay_us(10);
    	i2c_scl_high();
    	os_delay_us(10);
    }

    i2c_sda_high();
    os_delay_us(10);
    i2c_scl_high();
    os_delay_us(10);
    i2c_sda_low();
    os_delay_us(10);
    i2c_scl_low();
    os_delay_us(10);
    i2c_scl_high();
    os_delay_us(10);
    i2c_sda_high();
    os_delay_us(10);

    return;
}
