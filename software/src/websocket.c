/* WIFI Extension 2.0
 * Copyright (C) 2014-2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * websocket.c: Websocket implementation (Originally from Master Brick)
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

#include "websocket.h"

#include "config.h"
#include "espmissingincludes.h"

#include "base64_encode.h"

#include "logging.h"

bool websocket_found_key = false;

// Reimplement strcasestr since we don't have any std lib on ESP8266
int ICACHE_FLASH_ATTR tolower(int chr) {
    return (chr >='A' && chr<='Z') ? (chr + 32) : (chr);
}

char* ICACHE_FLASH_ATTR strcasestr(char *haystack, char *needle) {
	char *p, *startn = 0, *np = 0;

	for(p = haystack; *p; p++) {
		if(np) {
			if(tolower(*p) == tolower(*np)) {
				if(!*++np) {
					return startn;
				}
			} else {
				np = 0;
			}
		} else if(tolower(*p) == tolower(*needle)) {
			np = needle + 1;
			startn = p;
		}
	}

	return 0;
}

// Use SHA1 functins that are available in ESP8266 code to implement sha1
unsigned char * ICACHE_FLASH_ATTR SHA1(const unsigned char *d, size_t n, unsigned char *md) {
	SHA1_CTX ctx;

	SHA1Init(&ctx);
	SHA1Update(&ctx, d, n);
	SHA1Final((void *)md, &ctx);

	return md;
}

void ICACHE_FLASH_ATTR websocket_parse_handshake_line(char *line, uint8_t length, websocket_answer_callback_t websocket_answer_callback, websocket_answer_error_t websocket_answer_error, int32_t user_data) {
	static char key[WEBSOCKET_KEY_LEN] = {0};

	// Find "\r\n"
	for(uint8_t i = 0; i < length; i++) {
		if(line[i] == ' ' || line[i] == '\t') {
			continue;
		}

		if(line[i] == '\r' && line[i+1] == '\n') {
			if(!websocket_found_key) {
				websocket_answer_error(user_data);
				return;
			}
			websocket_found_key = false;

			// Concatenate client and server key
			char concatkey[WEBSOCKET_CONCATKEY_LEN] = {0};
			strcpy(concatkey, key);
			strcpy(concatkey+strlen(key), WEBSOCKET_SERVER_KEY);

			// Calculate sha1 hash
			char hash[20] = {0};
			SHA1((unsigned char*)concatkey, strlen(concatkey), (unsigned char*)hash);

			// Calculate base64 from hash
			memset(concatkey, 0, WEBSOCKET_CONCATKEY_LEN);
			int32_t base64_length = base64_encode_string(hash, 20, concatkey, WEBSOCKET_CONCATKEY_LEN);

			websocket_answer_callback(concatkey, base64_length, user_data);
			return;
		} else {
			break;
		}
	}

	// Find "Sec-WebSocket-Key"
	char *ret = strcasestr(line, WEBSOCKET_CLIENT_KEY_STR);
	if(ret != NULL) {
		memset(key, 0, WEBSOCKET_KEY_LEN);
		uint8_t concat_i = 0;
		for(uint8_t i = strlen(WEBSOCKET_CLIENT_KEY_STR); i < length; i++) {
			if(line[i] != ' ' && line[i] != '\n' && line[i] != '\r') {
				key[concat_i] = line[i];
				concat_i++;
			}
		}

		websocket_found_key = true;

		return;
	}
}

void ICACHE_FLASH_ATTR websocket_parse_handshake(char *header_part, uint8_t length, websocket_answer_callback_t websocket_answer_callback, websocket_answer_error_t websocket_answer_error, int32_t user_data) {
	static char line[MAX_WEBSOCKET_LINE_LENGTH] = {0};
	static uint8_t line_index = 0;

	for(uint8_t i = 0; i < length; i++) {
		// If line is > WEBSOCKET_MAX_LINE_LENGTH we just read over it until we find '\n'
		// The lines we are interested in will not be that long
		if(line_index < MAX_WEBSOCKET_LINE_LENGTH-1) {
			line[line_index] = header_part[i];
			line_index++;
		}
		if(header_part[i] == '\n') {
			websocket_parse_handshake_line(line, line_index, websocket_answer_callback, websocket_answer_error, user_data);
			memset(line, 0, MAX_WEBSOCKET_LINE_LENGTH);
			line_index = 0;
		}
	}
}

void ICACHE_FLASH_ATTR websocket_debug_header(const WebsocketFrame *wsf) {
	logwohd("WebSocket Header: fin: %d", wsf->fin);
	logwohd(", rsv1: %d", wsf->rsv1);
	logwohd(", rsv2: %d", wsf->rsv2);
	logwohd(", rsv3: %d", wsf->rsv3);
	logwohd(", opc: %d", wsf->opcode);
	logwohd(", len: %d", wsf->payload_length);
	logwohd(", key: (%x, %x, %x, %x)\n\r", wsf->masking_key[0], wsf->masking_key[1], wsf->masking_key[2], wsf->masking_key[3]);
}
