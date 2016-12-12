/* WIFI Extension 2.0
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * configuration.h: Mesh networking implementation
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

#ifndef TFP_MESH_CONNECTION_H
#define TFP_MESH_CONNECTION_H

#include "c_types.h"
#include "ringbuffer.h"
#include "tfp_connection.h"
#include "user_interface.h"

#define TFP_MESH_SEND_RING_BUFFER_SIZE 10240 // 128 full size (80 bytes) TFP packets.
#define FMT_MESH_STATION_HOSTNAME "%s_C_%X%X%X"

extern Ringbuffer tfp_mesh_send_rb;
extern uint8_t tfp_mesh_send_rb_buffer[TFP_MESH_SEND_RING_BUFFER_SIZE];

void ICACHE_FLASH_ATTR tfp_mesh_open_connection(void);
void ICACHE_FLASH_ATTR tfp_mesh_send_clear_buffer(void);
bool ICACHE_FLASH_ATTR tfp_mesh_send_check_buffer(uint8_t len);
int8_t ICACHE_FLASH_ATTR tfp_mesh_send(void *arg, uint8_t *data, uint8_t length);

// Callbacks.
void cb_tmr_tfp_mesh_stat(void);
void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac);
void ICACHE_FLASH_ATTR cb_tfp_mesh_disconnect(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status);
void ICACHE_FLASH_ATTR cb_tfp_mesh_reconnect(void *arg, sint8 error);
void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata, uint16_t len);

#endif
