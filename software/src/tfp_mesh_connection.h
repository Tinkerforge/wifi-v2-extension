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

#define MESH_PACKET_HELLO  1
#define MESH_PACKET_OLLEH  2
#define MESH_PACKET_HB_REQ 3
#define MESH_PACKET_HB_RES 4
#define MESH_PACKET_TFP    5

#define TFP_MESH_MIN_LENGTH 16
#define TFP_MESH_HEADER_LENGTH_INDEX 3
// 128 full size (80 bytes) TFP packets.
#define TFP_MESH_SEND_RING_BUFFER_SIZE 10240
#define FMT_MESH_STATION_HOSTNAME "%s_C_%X%X%X"

typedef struct {
  uint8_t firmware_version[3];
  char prefix[CONFIGURATION_SSID_MAX_LENGTH / 2];
  uint8_t group_id[6];
} __attribute__((packed)) pkt_mesh_hello_t;

void ICACHE_FLASH_ATTR init_tfp_con_mesh(void);
void ICACHE_FLASH_ATTR tfp_mesh_open_connection(void);
void ICACHE_FLASH_ATTR tfp_mesh_send_buffer_clear(void);
bool ICACHE_FLASH_ATTR tfp_mesh_send_buffer_check(uint8_t len);
int8_t ICACHE_FLASH_ATTR tfp_mesh_send(void *arg,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint8_t packet_type);
bool ICACHE_FLASH_ATTR tfp_mesh_send_handler(const uint8_t *data, uint8_t length);

// Callbacks.
void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac);
void ICACHE_FLASH_ATTR cb_tfp_mesh_disconnect(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status);
void ICACHE_FLASH_ATTR cb_tfp_mesh_disable(int8_t status);
void ICACHE_FLASH_ATTR cb_tfp_mesh_reconnect(void *arg, sint8 error);
void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata, uint16_t len);

#endif
