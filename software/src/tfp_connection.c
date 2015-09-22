/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * tfp_connection.c: Implementation of connection for Tinkerforge protocol
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

#include "tfp_connection.h"

#include "gpio.h"
#include "espmissingincludes.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "uart_connection.h"
#include "brickd.h"
#include "ringbuffer.h"
#include "communication.h"
#include "configuration.h"
#include "user_interface.h"
#include "logging.h"

Ringbuffer tfp_rb;
uint8_t tfp_rb_buffer[TFP_RING_BUFFER_SIZE];

TFPConnection tfp_cons[TFP_MAX_CONNECTIONS];

extern Configuration configuration_current;

#define TFP_RECV_INDEX_LENGTH 4
#define TFP_MIN_LENGTH 8


void ICACHE_FLASH_ATTR tfp_init_con(int8_t cid) {
	tfp_cons[cid].recv_buffer_index = 0;
	tfp_cons[cid].recv_buffer_expected_length = TFP_MIN_LENGTH;
	tfp_cons[cid].state = TFP_CON_STATE_CLOSED;
	tfp_cons[cid].cid   = cid;
	tfp_cons[cid].con   = NULL;
	if(configuration_current.general_authentication_secret[0] != '\0') {
		tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_ENABLED;
	} else {
		tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_DISABLED;
	}
}

void ICACHE_FLASH_ATTR tfp_sent_callback(void *arg) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;
	tfp_con->state = TFP_CON_STATE_OPEN;
}

void ICACHE_FLASH_ATTR tfp_handle_packet(const uint8_t *data, const uint8_t length) {
	// If ringbuffer not empty we add data to ringbuffer (we want to keep order)
	// Else if we can't immediately send to master brick we also add to ringbuffer
	if(!ringbuffer_is_empty(&tfp_rb) || uart_con_send(data, length) == 0) {

		// TODO: Test free size
		for(uint8_t i = 0; i < length; i++) {
			bool ret_ok = ringbuffer_add(&tfp_rb, data[i]);
			if(!ret_ok) {
				loge("ringbuffer full!\n");
			}
		}
	}
}

void ICACHE_FLASH_ATTR tfp_recv_callback(void *arg, char *pdata, unsigned short len) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	// os_printf("tfp_recv_callback: %d\n", len);
	for(uint32_t i = 0; i < len; i++) {
		tfp_con->recv_buffer[tfp_con->recv_buffer_index] = pdata[i];
		if(tfp_con->recv_buffer_index == TFP_RECV_INDEX_LENGTH) {
			// TODO: Sanity-check length
			tfp_con->recv_buffer_expected_length = tfp_con->recv_buffer[TFP_RECV_INDEX_LENGTH];
		}

		tfp_con->recv_buffer_index++;
		if(tfp_con->recv_buffer_index == tfp_con->recv_buffer_expected_length) {
			if(!com_handle_message(tfp_con->recv_buffer, tfp_con->recv_buffer_expected_length, tfp_con->cid)) {
				brickd_route_from(tfp_con->recv_buffer, tfp_con->cid);
				// os_printf("tfp_recv -> d:%d, l:%d, c:%d)\n", tfp_con->recv_buffer[8], tfp_con->recv_buffer_expected_length, tfp_con->cid);

				tfp_handle_packet(tfp_con->recv_buffer, tfp_con->recv_buffer_expected_length);

			}

			tfp_con->recv_buffer_index = 0;
			tfp_con->recv_buffer_expected_length = TFP_MIN_LENGTH;
		}
	}
}

void ICACHE_FLASH_ATTR tfp_disconnect_callback(void *arg) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	brickd_disconnect(tfp_con->cid);

	tfp_init_con(tfp_con->cid);

	os_printf("tfp_disconnect_callback: cid %d\n", tfp_con->cid);
}

void ICACHE_FLASH_ATTR tfp_reconnect_callback(void *arg, sint8 error) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	brickd_disconnect(tfp_con->cid);

	tfp_init_con(tfp_con->cid);

	if(error == ESPCONN_TIMEOUT) {
		// espconn_disconnect(arg);
	}

	os_printf("tfp_reconnect_callback: cid %d -> %d\n", tfp_con->cid, error);
}

void ICACHE_FLASH_ATTR tfp_connect_callback(void *arg) {
	uint8_t i = 0;
	for(; i < TFP_MAX_CONNECTIONS; i++) {
		if(tfp_cons[i].state == TFP_CON_STATE_CLOSED) {
			tfp_cons[i].con = arg;
			tfp_cons[i].con->reverse = &tfp_cons[i];
			tfp_cons[i].state = TFP_CON_STATE_OPEN;
			os_printf("tfp_connect_callback: cid %d\n", tfp_cons[i].cid);
			break;
		}
	}

	if(i == TFP_MAX_CONNECTIONS) {
		os_printf("Too many open connections, can't handle more\n");
		// TODO: according to the documentation we can not call espconn_disconnect in callback
		// espconn_disconnect(arg);
		return;
	}

/*	uint8_t param = 10;
	espconn_set_keepalive(arg, ESPCONN_KEEPIDLE, &param);
	param = 2;
	espconn_set_keepalive(arg, ESPCONN_KEEPINTVL, &param);
	param = 10;
	espconn_set_keepalive(arg, ESPCONN_KEEPCNT, &param);

	espconn_set_opt(arg, ESPCONN_KEEPALIVE);
	espconn_set_opt(arg, ESPCONN_REUSEADDR);
	espconn_set_opt(arg, ESPCONN_COPY);
	espconn_set_opt(arg, ESPCONN_NODELAY);*/

//	espconn_set_opt(arg, ESPCONN_COPY);

	espconn_set_opt(arg, ESPCONN_NODELAY);

	espconn_regist_recvcb(arg, tfp_recv_callback);
	espconn_regist_disconcb(arg, tfp_disconnect_callback);
	espconn_regist_sentcb(arg, tfp_sent_callback);
}

static bool ICACHE_FLASH_ATTR tfp_send_check_buffer(const uint8_t *data, const uint8_t length, const int8_t cid) {
	if(cid == -1) { // Broadcast
		for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
			if(tfp_cons[i].state == TFP_CON_STATE_SENDING) {
				// If we need to broadcast and one of the recipients has a full
				// buffer, we have to return false
				return false;
			}
		}
	} else {
		if(tfp_cons[cid].state == TFP_CON_STATE_SENDING) {
			return false;
		}
	}

	return true;
}

bool ICACHE_FLASH_ATTR tfp_send_w_cid(const uint8_t *data, const uint8_t length, const uint8_t cid) {
	if(tfp_cons[cid].state == TFP_CON_STATE_OPEN) {
		tfp_cons[cid].state = TFP_CON_STATE_SENDING;
		os_memcpy(tfp_cons[cid].send_buffer, data, length);
		const uint8_t status = espconn_sent(tfp_cons[cid].con, (uint8_t*)data, length);
		// TODO: Check status?
		return true;
	}

	return false;
}

bool ICACHE_FLASH_ATTR tfp_send(const uint8_t *data, const uint8_t length) {

	//os_printf("tfp_send with length %d\n", length);

	// TODO: Sanity check length again?

	// TODO: Are we sure that data is always a full TFP packet?

	// cid == -2 => send back via UART
	if(com_handle_message(data, length, -2)) {
		return true;
	}

	// We only peak at the routing table here (and delete the route manually if
	// we can fit the data in our buffer). It would be very expensive to first
	// peak and then discover the route again.
	BrickdRouting *match = NULL;
	int8_t cid = brickd_route_to_peak(data, &match);

	// First lets check if everything fits in the buffers
	if(!tfp_send_check_buffer(data, length, cid)) {
		//os_printf("bf (c %d)\n", cid);
		return false;
	}

	// Remove match from brickd routing table only if we now that we can fit
	// the data in the buffer
	if(match != NULL) {
		match->uid = 0;
		match->func_id = 0;
		match->sequence_number = 0;
		match->cid = -1;
	}


	//os_printf("tfp_send found cid %d\n", cid);
	if(cid == -1) { // Broadcast
		// os_printf("cid == -1\n");
		for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
			if(tfp_cons[i].state == TFP_CON_STATE_OPEN) {
				// TODO: sent data (return value)
				//os_printf("tfp_send to cid %d (broadcast)\n", i);
				tfp_cons[i].state = TFP_CON_STATE_SENDING;
				os_memcpy(tfp_cons[i].send_buffer, data, length);
				espconn_sent(tfp_cons[i].con, tfp_cons[i].send_buffer, length);
			}
		}
	} else {
		//os_printf("tfp_send to cid %d\n", cid);
		tfp_cons[cid].state = TFP_CON_STATE_SENDING;
		os_memcpy(tfp_cons[cid].send_buffer, data, length);
		const uint8_t status = espconn_sent(tfp_cons[cid].con, (uint8_t*)data, length);
		// os_printf("tfp_send -> d:%d, s:%d, l:%d, c:%d)\n", data[8], status, length, cid);
	}

	return true;
	// os_printf("tfp_send: %d\n", length_send);
}

// TODO: Does this have to be available after espconn_regist_time call?
//       I.e. can we just have it locally in the function?
static espconn tfp_con_listen;
static esp_tcp tfp_con_listen_tcp;

void ICACHE_FLASH_ATTR tfp_open_connection(void) {
	ringbuffer_init(&tfp_rb, tfp_rb_buffer, TFP_RING_BUFFER_SIZE);
	brickd_init();

	ets_memset(&tfp_con_listen, 0, sizeof(struct espconn));

	for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
		tfp_init_con(i);
	}

	// Initialize the ESPConn
	espconn_create(&tfp_con_listen);
	tfp_con_listen.type = ESPCONN_TCP;
	tfp_con_listen.state = ESPCONN_NONE;

	// Make it a TCP connection
	tfp_con_listen.proto.tcp = &tfp_con_listen_tcp;
	tfp_con_listen.proto.tcp->local_port = configuration_current.general_port;

	espconn_regist_reconcb(&tfp_con_listen, tfp_reconnect_callback);
	espconn_regist_connectcb(&tfp_con_listen, tfp_connect_callback);

	// Start listening
	espconn_accept(&tfp_con_listen);

	// Set server timeout (in seconds)
	espconn_regist_time(&tfp_con_listen, 7200, 0);
}

void ICACHE_FLASH_ATTR tfp_poll(void) {
	if(ringbuffer_is_empty(&tfp_rb)) {
		return;
	}

	uint8_t data[TFP_RECV_BUFFER_SIZE];
	uint8_t length = ringbuffer_peak(&tfp_rb, data, TFP_RECV_BUFFER_SIZE);
	// os_printf("tfp_poll peak: %d %d\n", length, data[TFP_RECV_INDEX_LENGTH]);

	if(length > TFP_RECV_BUFFER_SIZE) {
		ringbuffer_init(&tfp_rb, tfp_rb.buffer, tfp_rb.buffer_length);
		os_printf("tfp_poll: length > %d: %d\n", TFP_RECV_BUFFER_SIZE, length);
		return;
	}

	if(length < TFP_MIN_LENGTH) {
		ringbuffer_init(&tfp_rb, tfp_rb.buffer, tfp_rb.buffer_length);
		os_printf("tfp_poll: length < TFP_MIN_LENGTH\n");
		return;
	}

	/*if(length != data[TFP_RECV_INDEX_LENGTH]) {
		os_printf("tfp_poll: l (%d) != d[TFP] (%d)\n", length, data[TFP_RECV_INDEX_LENGTH]);
		os_printf("data: [%d %d %d %d %d]\n", data[0], data[1], data[2], data[3], data[4]);

		ringbuffer_init(&tfp_rb, tfp_rb.buffer, tfp_rb.buffer_length);
		return;
	}*/

	// TODO: Are we sure that this is the only place where ringbuffer_get(&tfp_rb, ...) is called?
	if(uart_con_send(data, data[TFP_RECV_INDEX_LENGTH]) != 0) {
		// os_printf("tfp_poll send ok\n");
		uint8_t dummy;
		for(uint8_t i = 0; i < data[TFP_RECV_INDEX_LENGTH]; i++) {
			ringbuffer_get(&tfp_rb, &dummy);
		}
	} else {
		os_printf("tfp_poll send error\n");
	}
}
