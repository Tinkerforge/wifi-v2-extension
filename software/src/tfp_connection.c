/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
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

// Mesh related.
#include "tfp_mesh_connection.h"

Ringbuffer tfp_rb;
uint8_t tfp_rb_buffer[TFP_RING_BUFFER_SIZE];

TFPConnection tfp_cons[TFP_MAX_CONNECTIONS];
bool tfp_is_hold = false;

extern GetWifi2StatusReturn gw2sr;
extern Configuration configuration_current;


void ICACHE_FLASH_ATTR tfp_init_con(const int8_t cid) {
	tfp_cons[cid].recv_buffer_index = 0;
	tfp_cons[cid].recv_buffer_expected_length = TFP_MIN_LENGTH;
	tfp_cons[cid].state = TFP_CON_STATE_CLOSED;
	tfp_cons[cid].cid   = cid;
	tfp_cons[cid].con   = NULL;
	tfp_cons[cid].websocket_state = WEBSOCKET_STATE_NO_WEBSOCKET;
	if(configuration_current.general_authentication_secret[0] != '\0') {
		tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_ENABLED;
	} else {
		tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_DISABLED;
	}
}

void ICACHE_FLASH_ATTR tfp_sent_callback(void *arg) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	packet_counter(con, PACKET_COUNT_TX);

	if(tfp_con->state == TFP_CON_STATE_CLOSED_AFTER_SEND) {
		espconn_disconnect(tfp_con->con);
	} else {
		tfp_con->state = TFP_CON_STATE_OPEN;
	}
}

void ICACHE_FLASH_ATTR tfp_handle_packet(const uint8_t *data, const uint8_t length) {
	// If ringbuffer not empty we add data to ringbuffer (we want to keep order)
	// Else if we can't immediately send to master brick we also add to ringbuffer
	if(!ringbuffer_is_empty(&tfp_rb) || uart_con_send(data, length) == 0) {

		if(ringbuffer_get_free(&tfp_rb) > (length + 2)) {
			for(uint8_t i = 0; i < length; i++) {
				if(!ringbuffer_add(&tfp_rb, data[i])) {
					// Should be unreachable
					loge("Ringbuffer full!\n");
				}
			}
		} else {
			logw("Message does not fit in Buffer: %d < %d\n", ringbuffer_get_free(&tfp_rb), length + 2);
		}
	}
}

void ICACHE_FLASH_ATTR tfp_recv_hold(void) {
	if(!tfp_is_hold) {
		for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
			if(tfp_cons[i].state == TFP_CON_STATE_OPEN || tfp_cons[i].state == TFP_CON_STATE_SENDING) {
				espconn_recv_hold(tfp_cons[i].con);
			}
		}

		logd("hold: %d\n\r", ringbuffer_get_free(&tfp_rb));
		tfp_is_hold = true;
	}
}

void ICACHE_FLASH_ATTR tfp_recv_unhold(void) {
	if(tfp_is_hold) {
		for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
			if(tfp_cons[i].state == TFP_CON_STATE_OPEN || tfp_cons[i].state == TFP_CON_STATE_SENDING) {
				espconn_recv_unhold(tfp_cons[i].con);
			}
		}

		logd("unhold: %d\n\r", ringbuffer_get_free(&tfp_rb));
		tfp_is_hold = false;
	}
}

void ICACHE_FLASH_ATTR tfp_recv_callback(void *arg, char *pdata, unsigned short len) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	packet_counter(con, PACKET_COUNT_RX);

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
				tfp_handle_packet(tfp_con->recv_buffer, tfp_con->recv_buffer_expected_length);
			}

			tfp_con->recv_buffer_index = 0;
			tfp_con->recv_buffer_expected_length = TFP_MIN_LENGTH;
		}
	}

	if(ringbuffer_get_free(&tfp_rb) < (5*MTU_LENGTH + 2)) {
		tfp_recv_hold();
	}
}

void ICACHE_FLASH_ATTR tfp_disconnect_callback(void *arg) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	brickd_disconnect(tfp_con->cid);

	tfp_init_con(tfp_con->cid);

	logd("tfp_disconnect_callback: cid %d\n", tfp_con->cid);
}

void ICACHE_FLASH_ATTR tfp_reconnect_callback(void *arg, sint8 error) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	brickd_disconnect(tfp_con->cid);

	tfp_init_con(tfp_con->cid);

	if(error == ESPCONN_TIMEOUT) {
		// espconn_disconnect(arg);
	}

	logd("tfp_reconnect_callback: cid %d -> %d\n", tfp_con->cid, error);
}

void ICACHE_FLASH_ATTR tfp_connect_callback(void *arg) {
	uint8_t i = 0;
	for(; i < TFP_MAX_CONNECTIONS; i++) {
		if(tfp_cons[i].state == TFP_CON_STATE_CLOSED) {
			tfp_cons[i].con = arg;
			tfp_cons[i].con->reverse = &tfp_cons[i];
			tfp_cons[i].state = TFP_CON_STATE_OPEN;
			logd("tfp_connect_callback: cid %d\n", tfp_cons[i].cid);
			break;
		}
	}

	if(i == TFP_MAX_CONNECTIONS) {
		logw("Too many open connections, can't handle more\n");
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

static bool ICACHE_FLASH_ATTR tfp_send_check_buffer(const int8_t cid) {
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
		espconn_send(tfp_cons[cid].con, (uint8_t*)data, length);
		return true;
	}

	return false;
}

bool ICACHE_FLASH_ATTR tfp_send(const uint8_t *data, const uint8_t length) {
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

	if(!brickd_check_auth((const MessageHeader*)data, cid)) {
		return true;
	}

	// First lets check if everything fits in the buffers
	if(!tfp_send_check_buffer(cid)) {
		return false;
	}

	// Add websocket header if necessary
	uint8_t data_with_websocket_header[TFP_SEND_BUFFER_SIZE + sizeof(WebsocketFrameClientToServer)];
	int16_t length_with_websocket_header = tfpw_insert_websocket_header(cid, data_with_websocket_header, data, length);
	if(length_with_websocket_header == -1) { // -1 = We use websocket but state is not OK for sending
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

	if(cid == -1) { // Broadcast
		for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
			if(tfp_cons[i].state == TFP_CON_STATE_OPEN) {
				// TODO: sent data (return value)
				tfp_cons[i].state = TFP_CON_STATE_SENDING;
				uint8_t length_to_send = length;
				if(tfp_cons[i].websocket_state == WEBSOCKET_STATE_NO_WEBSOCKET) {
					os_memcpy(tfp_cons[i].send_buffer, data, length);
				} else {
					os_memcpy(tfp_cons[i].send_buffer, data_with_websocket_header, length_with_websocket_header);
					length_to_send = length_with_websocket_header;
				}
				espconn_send(tfp_cons[i].con, tfp_cons[i].send_buffer, length_to_send);
			}
		}
	} else {
		tfp_cons[cid].state = TFP_CON_STATE_SENDING;

		uint8_t length_to_send = length;
		if(tfp_cons[cid].websocket_state == WEBSOCKET_STATE_NO_WEBSOCKET) {
			os_memcpy(tfp_cons[cid].send_buffer, data, length);
		} else {
			os_memcpy(tfp_cons[cid].send_buffer, data_with_websocket_header, length_with_websocket_header);
			length_to_send = length_with_websocket_header;
		}
		espconn_send(tfp_cons[cid].con, tfp_cons[cid].send_buffer, length_to_send);
	}

	if(ringbuffer_get_free(&tfp_rb) > (6*MTU_LENGTH + 2)) {
		tfp_recv_unhold();
	}

	return true;
}

// TODO: Does this have to be available after espconn_regist_time call?
//       I.e. can we just have it locally in the function?
static espconn tfp_con_listen;
static esp_tcp tfp_con_listen_tcp;

void ICACHE_FLASH_ATTR tfp_open_connection(void) {
	// Common initialisations.
	ringbuffer_init(&tfp_rb, tfp_rb_buffer, TFP_RING_BUFFER_SIZE);
	brickd_init();
	com_init();

	for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
		tfp_init_con(i);
	}

	#if(MESH_ENABLED == 1)
		// Start mesh.
		tfp_mesh_enable();
	#else
		ets_memset(&tfp_con_listen, 0, sizeof(struct espconn));

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
	#endif
}

void ICACHE_FLASH_ATTR tfp_poll(void) {
	if(ringbuffer_is_empty(&tfp_rb)) {
		return;
	}

	uint8_t data[TFP_RECV_BUFFER_SIZE];
	uint8_t length = ringbuffer_peak(&tfp_rb, data, TFP_RECV_BUFFER_SIZE);

	if(length > TFP_RECV_BUFFER_SIZE) {
		ringbuffer_init(&tfp_rb, tfp_rb.buffer, tfp_rb.buffer_length);
		logd("tfp_poll: length > %d: %d\n", TFP_RECV_BUFFER_SIZE, length);
		return;
	}

	if(length < TFP_MIN_LENGTH) {
		ringbuffer_init(&tfp_rb, tfp_rb.buffer, tfp_rb.buffer_length);
		logd("tfp_poll: length < TFP_MIN_LENGTH\n");
		return;
	}

	if(uart_con_send(data, data[TFP_RECV_INDEX_LENGTH]) != 0) {
		ringbuffer_remove(&tfp_rb, data[TFP_RECV_INDEX_LENGTH]);
	} else {
		logd("tfp_poll send error\n");
	}
}

void ICACHE_FLASH_ATTR packet_counter(espconn *con, uint8_t direction) {
	uint8 ap_ip[4];
	uint8 ap_netmask[4];
	uint8 station_ip[4];
	uint8 station_netmask[4];
	uint8 connection_remote_ip[4];
	struct ip_info info_ap;
	struct ip_info info_station;

	switch(con->type) {
		case ESPCONN_TCP:
			connection_remote_ip[0] = con->proto.tcp->remote_ip[0];
			connection_remote_ip[1] = con->proto.tcp->remote_ip[1];
			connection_remote_ip[2] = con->proto.tcp->remote_ip[2];
			connection_remote_ip[3] = con->proto.tcp->remote_ip[3];
			break;

		case ESPCONN_UDP:
			connection_remote_ip[0] = con->proto.udp->remote_ip[0];
			connection_remote_ip[1] = con->proto.udp->remote_ip[1];
			connection_remote_ip[2] = con->proto.udp->remote_ip[2];
			connection_remote_ip[3] = con->proto.udp->remote_ip[3];
			break;

		default:
			return;
	}

	if(configuration_current.ap_enable) {
		wifi_get_ip_info(SOFTAP_IF, &info_ap);

		ap_ip[0] = ip4_addr1(&info_ap.ip);
		ap_ip[1] = ip4_addr2(&info_ap.ip);
		ap_ip[2] = ip4_addr3(&info_ap.ip);
		ap_ip[3] = ip4_addr4(&info_ap.ip);

		ap_netmask[0] = ip4_addr1(&info_ap.netmask);
		ap_netmask[1] = ip4_addr2(&info_ap.netmask);
		ap_netmask[2] = ip4_addr3(&info_ap.netmask);
		ap_netmask[3] = ip4_addr4(&info_ap.netmask);

		// Determine and match network address.
		if(((connection_remote_ip[0] & ap_netmask[0]) == \
			(ap_ip[0] & ap_netmask[0])) &&
		   ((connection_remote_ip[1] & ap_netmask[1]) == \
			(ap_ip[1] & ap_netmask[1])) &&
		   ((connection_remote_ip[2] & ap_netmask[2]) == \
			(ap_ip[2] & ap_netmask[2])) &&
		   ((connection_remote_ip[3] & ap_netmask[3]) == \
			(ap_ip[3] & ap_netmask[3]))) {
				if(direction == PACKET_COUNT_RX)
					gw2sr.ap_rx_count++;
				else if(direction == PACKET_COUNT_TX)
					gw2sr.ap_tx_count++;
		}
	}

	if(configuration_current.client_enable) {
		wifi_get_ip_info(STATION_IF, &info_station);

		station_ip[0] = ip4_addr1(&info_station.ip);
		station_ip[1] = ip4_addr2(&info_station.ip);
		station_ip[2] = ip4_addr3(&info_station.ip);
		station_ip[3] = ip4_addr4(&info_station.ip);

		station_netmask[0] = ip4_addr1(&info_station.netmask);
		station_netmask[1] = ip4_addr2(&info_station.netmask);
		station_netmask[2] = ip4_addr3(&info_station.netmask);
		station_netmask[3] = ip4_addr4(&info_station.netmask);

		// Determine and match network address.
		if(((connection_remote_ip[0] & station_netmask[0]) == \
			(station_ip[0] & station_netmask[0])) &&
		   ((connection_remote_ip[1] & station_netmask[1]) == \
			(station_ip[1] & station_netmask[1])) &&
		   ((connection_remote_ip[2] & station_netmask[2]) == \
			(station_ip[2] & station_netmask[2])) &&
		   ((connection_remote_ip[3] & station_netmask[3]) == \
			(station_ip[3] & station_netmask[3]))) {
				if(direction == PACKET_COUNT_RX)
					gw2sr.client_rx_count++;
				else if(direction == PACKET_COUNT_TX)
					gw2sr.client_tx_count++;
		}
	}
}
