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

#include "c_types.h"
#include "brickd.h"
#include "websocket.h"
#include "tfp_websocket_connection.h"

// The ESP8266 seems to allocate 1648 per message that is send.
// We make sure that it has at least 4 messages free space left at all times.
#define TFP_MIN_ESP_HEAP_SIZE (1648*4)

#define TFP_SEND_BUFFER_SIZE 80
#define TFP_RECV_BUFFER_SIZE 80
#define TFP_MAX_CONNECTIONS 5

#define TFP_RING_BUFFER_SIZE 10240
#define PACKET_COUNT_RX 1
#define PACKET_COUNT_TX 2
#define TFP_RECV_INDEX_LENGTH 4
#define TFP_MIN_LENGTH 8
#define TFP_MAX_LENGTH 80

#define MTU_LENGTH 1460

#define TFP_CON_STATE_CLOSED              0
#define TFP_CON_STATE_OPEN                1
#define TFP_CON_STATE_SENDING             2
#define TFP_CON_STATE_CLOSED_AFTER_SEND   3
#define TFP_CON_STATE_MESH_SENT_HELLO     4

typedef struct {
	uint8_t recv_buffer[TFP_RECV_BUFFER_SIZE\
	                    + sizeof(struct mesh_header_format)\
	                    + 1];
	uint8_t recv_buffer_index;
	uint8_t recv_buffer_expected_length;

	uint8_t send_buffer[TFP_SEND_BUFFER_SIZE\
	                    + sizeof(WebsocketFrameClientToServer)\
	                    + sizeof(struct mesh_header_format)\
	                    + 1];
	uint16_t send_buffer_length;
	uint8_t state;
	BrickdAuthenticationState brickd_authentication_state;

	TFPWebsocketState websocket_state;
	WebsocketFrame websocket_frame;
	uint8_t websocket_to_read;
	uint8_t websocket_mask_mod;

	int8_t cid;
	struct espconn *con;
} TFPConnection;

void ICACHE_FLASH_ATTR tfp_recv_callback(void *arg, char *pdata, unsigned short len);
void ICACHE_FLASH_ATTR tfp_reconnect_callback(void *arg, sint8 error);
void ICACHE_FLASH_ATTR tfp_connect_callback(void *arg);
void ICACHE_FLASH_ATTR tfp_disconnect_callback(void *arg);
void ICACHE_FLASH_ATTR tfp_write_finish_callback(void *arg);
void ICACHE_FLASH_ATTR tfp_init_con(const int8_t cid);
bool ICACHE_FLASH_ATTR tfp_send_w_cid(const uint8_t *data, const uint8_t length, const uint8_t cid);
void ICACHE_FLASH_ATTR tfp_handle_packet(const uint8_t *data, const uint8_t length);
void ICACHE_FLASH_ATTR tfp_open_connection(void);
bool ICACHE_FLASH_ATTR tfp_send(const uint8_t *data, const uint8_t length);
void ICACHE_FLASH_ATTR tfp_check_send_buffer(void);
void ICACHE_FLASH_ATTR tfp_poll(void);
void ICACHE_FLASH_ATTR packet_counter(struct espconn *con, uint8_t direction);

#endif
