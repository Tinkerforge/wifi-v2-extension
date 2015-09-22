/* ESP8266 drivers
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * i2c_master.h: Implementation of i2c master
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

#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"

#define I2C_MASTER_SDA_MUX PERIPHS_IO_MUX_GPIO2_U
#define I2C_MASTER_SCL_MUX PERIPHS_IO_MUX_MTMS_U
#define I2C_MASTER_SDA_GPIO 2
#define I2C_MASTER_SCL_GPIO 14
#define I2C_MASTER_SDA_FUNC FUNC_GPIO2
#define I2C_MASTER_SCL_FUNC FUNC_GPIO14

#define I2C_MASTER_READ 1
#define I2C_MASTER_WRITE 0

void  i2c_master_stop(void);
void  i2c_master_start(void);
uint8_t  i2c_master_recv(bool ack);
bool  i2c_master_send(const uint8_t value);
void  i2c_master_init(void);

#endif
