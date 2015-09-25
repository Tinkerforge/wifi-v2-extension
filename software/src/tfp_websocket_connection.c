/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * tfp_websocket_connection.c: Implementation of connection for
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

#include "tfp_websocket_connection.h"

#include "espmissingincludes.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "communication.h"
#include "configuration.h"
#include "user_interface.h"
#include "logging.h"
#include "tfp_connection.h"
#include "utility.h"

extern Configuration configuration_current;
extern TFPConnection tfp_cons[TFP_MAX_CONNECTIONS];

const char tfpw_answer_error_string[] = "HTTP/1.1 200 OK\r\nContent-Length: 276\r\nContent-Type: text/html\r\n\r\n<html><head><title>This is a Websocket</title></head><body>Dear Sir or Madam,<br/><br/>I regret to inform you that there is no webserver here.<br/>This port is exclusively used for Websockets.<br/><br/>Yours faithfully,<blockquote>WIFI Extension 2.0</blockquote></body></html>";

uint8_t tmp_data[300];

void tfpw_answer_error(int32_t user_data) {
	if(user_data >= TFP_MAX_CONNECTIONS) {
		return;
	}

	if(tfp_cons[user_data].state != TFP_CON_STATE_OPEN) {
		loge("tfpw_answer_error: %d\n", tfp_cons[user_data].state);
	}
	// We close the socket directly after the error string is send
	tfp_cons[user_data].state = TFP_CON_STATE_CLOSED_AFTER_SEND;
	espconn_sent(tfp_cons[user_data].con, (char*)tfpw_answer_error_string, sizeof(tfpw_answer_error_string));
}

void tfpw_answer_callback(char *answer, uint8_t length, int32_t user_data) {
	if(user_data >= TFP_MAX_CONNECTIONS) {
		return;
	}

	os_memcpy(tmp_data, WEBSOCKET_ANSWER_STRING_1, os_strlen(WEBSOCKET_ANSWER_STRING_1));
	os_memcpy(tmp_data + os_strlen(WEBSOCKET_ANSWER_STRING_1), answer, length);
	os_memcpy(tmp_data + os_strlen(WEBSOCKET_ANSWER_STRING_1) + length, WEBSOCKET_ANSWER_STRING_2, os_strlen(WEBSOCKET_ANSWER_STRING_2));

	if(tfp_cons[user_data].state != TFP_CON_STATE_OPEN) {
		loge("tfpw_answer_callback with invalid state: %d\n", tfp_cons[user_data].state);
	}
	tfp_cons[user_data].state = TFP_CON_STATE_SENDING;
	espconn_sent(tfp_cons[user_data].con, tmp_data, os_strlen(WEBSOCKET_ANSWER_STRING_1) + length + os_strlen(WEBSOCKET_ANSWER_STRING_2));
	tfp_cons[user_data].websocket_state = WEBSOCKET_STATE_HANDSHAKE_DONE;
	tfp_cons[user_data].websocket_to_read = sizeof(WebsocketFrame);
}

int16_t tfpw_insert_websocket_header(const int8_t cid, uint8_t *websocket_buffer, const uint8_t *buffer, const uint8_t length) {
	// In this cases the data wouldn't be used anyway, so we don't fill anything
	if(cid < 0) {
		uint8_t i;
		for(i = 0; i < TFP_MAX_CONNECTIONS; i++) {
			if(tfp_cons[i].websocket_state != WEBSOCKET_STATE_NO_WEBSOCKET) {
				break;
			}
		}
		if(i == TFP_MAX_CONNECTIONS) {
			return 0;
		}
	} else if(tfp_cons[cid].websocket_state == WEBSOCKET_STATE_NO_WEBSOCKET) {
		return 0;
	} /*else if(tfp_cons[cid].websocket_state != WEBSOCKET_STATE_WEBSOCKET_HEADER_DONE) {
		return -1;
	}*/

	WebsocketFrameClientToServer wfcts;
	wfcts.fin = 1;
	wfcts.rsv1 = 0;
	wfcts.rsv2 = 0;
	wfcts.rsv3 = 0;
	wfcts.opcode = 2;
	wfcts.mask = 0;
	wfcts.payload_length = length;

	os_memcpy((void*)websocket_buffer, (void*)&wfcts, sizeof(WebsocketFrameClientToServer));
	os_memcpy((void*)(websocket_buffer + sizeof(WebsocketFrameClientToServer)), buffer, length);

	return length + sizeof(WebsocketFrameClientToServer);
}

void tfpw_close_frame(const WebsocketFrame *wsf, const int8_t cid) {
	espconn_disconnect(tfp_cons[cid].con);
}


void ICACHE_FLASH_ATTR tfpw_sent_callback(void *arg) {
	tfp_sent_callback(arg);
}

void ICACHE_FLASH_ATTR tfpw_recv_callback(void *arg, char *pdata, unsigned short len) {
	espconn *con = (espconn *)arg;
	TFPConnection *tfp_con = (TFPConnection *)con->reverse;

	uint16_t pdata_read = 0;
	while(len > 0) {
		switch(tfp_con->websocket_state) {
			case WEBSOCKET_STATE_WAIT_FOR_HANDSHAKE: {
				uint8_t data[100];
				uint8_t handshake_length = MIN(100, len);
				if(handshake_length != 0) {
					len -= handshake_length;
					websocket_parse_handshake(pdata + pdata_read, handshake_length, tfpw_answer_callback, tfpw_answer_error, tfp_con->cid);
					pdata_read += handshake_length;
				}

				break;
			}

			case WEBSOCKET_STATE_HANDSHAKE_DONE: {
				// buffer with max possible size of websocket header
				tfp_con->websocket_frame.fin = 1;
				uint8_t to_read = MIN(tfp_con->websocket_to_read, len);
				len -= to_read;
				os_memcpy((uint8_t*)(&tfp_con->websocket_frame) + (sizeof(WebsocketFrame) - tfp_con->websocket_to_read),
				          pdata + pdata_read,
				          to_read);
				pdata_read += to_read;
				tfp_con->websocket_to_read -= to_read;

				if(tfp_con->websocket_to_read != 0) {
					break;
				}

				tfp_con->websocket_state = WEBSOCKET_STATE_WEBSOCKET_HEADER_DONE;

				if(tfp_con->websocket_frame.mask != 1) {
					// mask = 0 from browser is not allowed!
					loge("Mask!=1 not allowed: %d\n\r", tfp_con->websocket_frame.mask);
					espconn_disconnect(tfp_con->con);
					return;
				}

				// We currently don't support 16 bit or 64 bit length
				if(tfp_con->websocket_frame.payload_length == 126 || tfp_con->websocket_frame.payload_length == 127) {
					espconn_disconnect(tfp_con->con);
					return;
				}

				switch(tfp_con->websocket_frame.opcode) {
					case WEBSOCKET_OPCODE_CONTINUATION_FRAME:
					case WEBSOCKET_OPCODE_TEXT_FRAME: {
						// Continuation and text frame not allowed
						loge("Opcode not supported: %d\n\r", tfp_con->websocket_frame.opcode);
						espconn_disconnect(tfp_con->con);
						return;
					}

					case WEBSOCKET_OPCODE_BINARY_FRAME: {
						tfp_con->websocket_mask_mod = 0;
						tfp_con->websocket_to_read = tfp_con->websocket_frame.payload_length;
						break;
					}

					case WEBSOCKET_OPCODE_CLOSE_FRAME: {
						tfpw_close_frame(&tfp_con->websocket_frame, tfp_con->cid);
						break;
					}

					case WEBSOCKET_OPCODE_PING_FRAME: {
						loge("Opcode not supported: %d\n\r", tfp_con->websocket_frame.opcode);
						espconn_disconnect(tfp_con->con);
						break;
					}

					case WEBSCOKET_OPCODE_PONG_FRAME: {
						loge("Opcode not supported: %d\n\r", tfp_con->websocket_frame.opcode);
						espconn_disconnect(tfp_con->con);
						break;
					}
				}

				break;
			}

			case WEBSOCKET_STATE_WEBSOCKET_HEADER_DONE: {
				// Websocket header is done, we can just read the data

				uint16_t tfp_length = MIN(len, tfp_con->websocket_to_read);
				len -= tfp_length;

				for(uint8_t i = 0; i < tfp_length; i++) {
					pdata[pdata_read+i] ^= tfp_con->websocket_frame.masking_key[tfp_con->websocket_mask_mod];
					tfp_con->websocket_mask_mod++;
					if(tfp_con->websocket_mask_mod >= WEBSOCKET_MASK_LENGTH) {
						tfp_con->websocket_mask_mod = 0;
					}
				}

				tfp_con->websocket_to_read -= tfp_length;
				if(tfp_con->websocket_to_read == 0) {
					tfp_con->websocket_state = WEBSOCKET_STATE_HANDSHAKE_DONE;
					tfp_con->websocket_to_read = sizeof(WebsocketFrame);
				}

				tfp_recv_callback(arg, pdata + pdata_read, tfp_length);
				pdata_read += tfp_length;

				break;
			}

			default: {
				loge("Invalid Ethernet Websocket state: %d\n\r", tfp_con->websocket_state);
				break;
			}
		}
	}
}
void ICACHE_FLASH_ATTR tfpw_reconnect_callback(void *arg, sint8 error) {
	tfp_reconnect_callback(arg, error);
}

void ICACHE_FLASH_ATTR tfpw_disconnect_callback(void *arg) {
	tfp_disconnect_callback(arg);
}

void ICACHE_FLASH_ATTR tfpw_connect_callback(void *arg) {
	uint8_t i = 0;
	for(; i < TFP_MAX_CONNECTIONS; i++) {
		if(tfp_cons[i].state == TFP_CON_STATE_CLOSED) {
			tfp_cons[i].websocket_state = WEBSOCKET_STATE_WAIT_FOR_HANDSHAKE;
			tfp_cons[i].websocket_mask_mod = 0;
			tfp_cons[i].websocket_to_read = 0;
			tfp_cons[i].websocket_frame.fin = 0;
			tfp_cons[i].websocket_frame.mask = 0;
			tfp_cons[i].websocket_frame.rsv1 = 0;
			tfp_cons[i].websocket_frame.rsv2 = 0;
			tfp_cons[i].websocket_frame.rsv3 = 0;
			tfp_cons[i].websocket_frame.opcode = 0;
			tfp_cons[i].websocket_frame.payload_length = 0;
			tfp_cons[i].websocket_frame.masking_key[0] = 0;
			tfp_cons[i].websocket_frame.masking_key[1] = 0;
			tfp_cons[i].websocket_frame.masking_key[2] = 0;
			tfp_cons[i].websocket_frame.masking_key[3] = 0;
			tfp_cons[i].con = arg;
			tfp_cons[i].con->reverse = &tfp_cons[i];
			tfp_cons[i].state = TFP_CON_STATE_OPEN;
			break;
		}
	}

	if(i == TFP_MAX_CONNECTIONS) {
		logw("Too many open connections, can't handle more\n");
		// TODO: according to the documentation we can not call espconn_disconnect in callback
		// espconn_disconnect(arg);
		return;
	}

	espconn_set_opt(arg, ESPCONN_NODELAY);

	espconn_regist_recvcb(arg, tfpw_recv_callback);
	espconn_regist_disconcb(arg, tfpw_disconnect_callback);
	espconn_regist_sentcb(arg, tfpw_sent_callback);
}

static espconn tfpw_con_listen;
static esp_tcp tfpw_con_listen_tcp;

void ICACHE_FLASH_ATTR tfpw_open_connection(void) {
	ets_memset(&tfpw_con_listen, 0, sizeof(struct espconn));

	// Initialize the ESPConn
	espconn_create(&tfpw_con_listen);
	tfpw_con_listen.type = ESPCONN_TCP;
	tfpw_con_listen.state = ESPCONN_NONE;

	// Make it a TCP connection
	tfpw_con_listen.proto.tcp = &tfpw_con_listen_tcp;
	tfpw_con_listen.proto.tcp->local_port = configuration_current.general_websocket_port;

	espconn_regist_reconcb(&tfpw_con_listen, tfpw_reconnect_callback);
	espconn_regist_connectcb(&tfpw_con_listen, tfpw_connect_callback);

	// Start listening
	espconn_accept(&tfpw_con_listen);

	// Set server timeout (in seconds)
	espconn_regist_time(&tfpw_con_listen, 7200, 0);
}
