/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * tfp_websocket_connection.h: Implementation of connection for
 *                             Tinkerforge protocol over Websocket
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

#ifndef TFP_WEBSOCKET_CONNECTION_H
#define TFP_WEBSOCKET_CONNECTION_H

#include "config.h"
#include "c_types.h"
#include "websocket.h"

typedef enum {
	WEBSOCKET_STATE_WAIT_FOR_HANDSHAKE = 0,
	WEBSOCKET_STATE_HANDSHAKE_DONE,
	WEBSOCKET_STATE_WEBSOCKET_HEADER_DONE,
	WEBSOCKET_STATE_NO_WEBSOCKET
} TFPWebsocketState;

int16_t tfpw_insert_websocket_header(const int8_t cid, uint8_t *websocket_buffer, const uint8_t *buffer, const uint8_t length);
uint8_t tfpw_read_data_tcp(const uint8_t socket, uint8_t *buffer, const uint8_t length);
uint8_t tfpw_write_data_tcp(const uint8_t socket, const uint8_t *buffer, const uint8_t length);

void ICACHE_FLASH_ATTR tfpw_open_connection(void);

#endif
