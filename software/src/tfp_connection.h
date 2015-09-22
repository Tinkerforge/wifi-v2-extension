/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * tfp_connection.h: Implementation of connection for Tinkerforge protocol
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

#ifndef TFP_CONNECTION_H
#define TFP_CONNECTION_H

#include <stdint.h>
#include "c_types.h"

#include "brickd.h"


#define TFP_RING_BUFFER_SIZE 1000

#define TFP_SEND_BUFFER_SIZE 80
#define TFP_RECV_BUFFER_SIZE 80
#define TFP_MAX_CONNECTIONS 10

#define TFP_CON_STATE_CLOSED  0
#define TFP_CON_STATE_OPEN    1
#define TFP_CON_STATE_SENDING 2

typedef struct {
	uint8_t recv_buffer[TFP_RECV_BUFFER_SIZE];
	uint8_t recv_buffer_index;
	uint8_t recv_buffer_expected_length;

	uint8_t send_buffer[TFP_SEND_BUFFER_SIZE];
	uint8_t state;
	BrickdAuthenticationState brickd_authentication_state;

	int8_t cid;
	espconn *con;
} TFPConnection;

bool ICACHE_FLASH_ATTR tfp_send_w_cid(const uint8_t *data, const uint8_t length, const uint8_t cid);
void ICACHE_FLASH_ATTR tfp_handle_packet(const uint8_t *data, const uint8_t length);
void ICACHE_FLASH_ATTR tfp_open_connection(void);
bool ICACHE_FLASH_ATTR tfp_send(const uint8_t *data, const uint8_t length);
void ICACHE_FLASH_ATTR tfp_poll(void);

#endif
