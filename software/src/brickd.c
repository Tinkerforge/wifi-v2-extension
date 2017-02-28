/* master-brick
 * Copyright (C) 2012-2013 Olaf Lüke <olaf@tinkerforge.com>
 *
 * brickd.c: Simplistic reimplementation of brickd for WIFI Extension
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

#define _GNU_SOURCE // for strnlen
#undef __STRICT_ANSI__ // for strnlen

#include <string.h> // for strnlen

#include "brickd.h"

#include "logging.h"

#include "espmissingincludes.h"
#include "osapi.h"

#include "tfp_connection.h"
#include "communication.h"
#include "configuration.h"

#include "ip_addr.h"
#include "espconn.h"

#define SHA1_DIGEST_LENGTH 20
#define SHA1_BLOCK_LENGTH 64

extern Configuration configuration_current;
extern TFPConnection tfp_cons[TFP_MAX_CONNECTIONS];
BrickdRouting brickd_routing_table[BRICKD_ROUTING_TABLE_SIZE];

uint32_t brickd_counter = 0;
uint32_t brickd_authentication_nonce = 42; // Will be randomized

void ICACHE_FLASH_ATTR brickd_init(void) {
	for(uint8_t i = 0; i < BRICKD_ROUTING_TABLE_SIZE; i++) {
		brickd_remove_route(&brickd_routing_table[i]);
	}
}

uint32_t ICACHE_FLASH_ATTR brickd_counter_diff(const uint32_t new, const uint32_t old) {
	if(new >= old) {
		return new - old;
	}

	return new + (0xFFFFFFFFL - old);
}

void ICACHE_FLASH_ATTR brickd_remove_route(BrickdRouting *route) {
	if(route != NULL) {
		route->uid = 0;
		route->counter = 0;
		route->func_id = 0;
		route->sequence_number = 0;
		route->cid = -1;
	}
}

void ICACHE_FLASH_ATTR brickd_route_from(const void *data, const uint8_t cid) {
	MessageHeader *data_header = (MessageHeader*)data;

	// Broadcast
	if(data_header->sequence_num == 0 || data_header->uid == 0 || !data_header->return_expected) {
		return;
	}

	BrickdRouting *oldest = &brickd_routing_table[0];
	brickd_counter++;
	uint32_t diff = brickd_counter_diff(brickd_counter, oldest->counter);

	for(uint8_t i = 0; i < BRICKD_ROUTING_TABLE_SIZE; i++) {
		if(brickd_routing_table[i].cid == -1) {
			brickd_routing_table[i].uid = data_header->uid;
			brickd_routing_table[i].func_id = data_header->fid;
			brickd_routing_table[i].sequence_number = data_header->sequence_num;
			brickd_routing_table[i].cid = cid;
			brickd_routing_table[i].counter = brickd_counter;
			brickd_routing_table[i].tmp = ((uint8_t*)data)[8];
			return;
		} else {
			uint32_t new_diff = brickd_counter_diff(brickd_counter, brickd_routing_table[i].counter);
			if(new_diff > diff) {
				oldest = &brickd_routing_table[i];
				diff = new_diff;
			}
		}
	}

	oldest->uid = data_header->uid;
	oldest->func_id = data_header->fid;
	oldest->sequence_number = data_header->sequence_num;
	oldest->cid = cid;
	oldest->counter = brickd_counter;
}

int8_t ICACHE_FLASH_ATTR brickd_route_to_peak(const void *data, BrickdRouting **match) {
	MessageHeader *data_header = (MessageHeader*)data;

	// Broadcast
	if(data_header->sequence_num == 0 || data_header->uid == 0) {
		return -1;
	}

	*match = NULL;
	uint32_t current_diff = 0;

	for(uint8_t i = 0; i < BRICKD_ROUTING_TABLE_SIZE; i++) {
		if(brickd_routing_table[i].uid == data_header->uid &&
		   brickd_routing_table[i].func_id == data_header->fid &&
		   brickd_routing_table[i].sequence_number == data_header->sequence_num) {
			if(*match == NULL) {
				*match = &brickd_routing_table[i];
				current_diff = brickd_counter_diff(brickd_counter, (*match)->counter);
			} else {
				uint32_t new_diff = brickd_counter_diff(brickd_counter, brickd_routing_table[i].counter);
				if(new_diff > current_diff) {
					*match = &brickd_routing_table[i];
					current_diff = new_diff;
				}
			}
		}
	}

	if(*match != NULL) {
		return (*match)->cid;
	}

	return -1;
}

int8_t ICACHE_FLASH_ATTR brickd_route_to(const void *data) {
	MessageHeader *data_header = (MessageHeader*)data;

	// Broadcast
	if(data_header->sequence_num == 0 || data_header->uid == 0) {
		return -1;
	}

	BrickdRouting *current_match = NULL;
	uint32_t current_diff = 0;

	for(uint8_t i = 0; i < BRICKD_ROUTING_TABLE_SIZE; i++) {
		if(brickd_routing_table[i].uid == data_header->uid &&
		   brickd_routing_table[i].func_id == data_header->fid &&
		   brickd_routing_table[i].sequence_number == data_header->sequence_num) {
			if(current_match == NULL) {
				current_match = &brickd_routing_table[i];
				current_diff = brickd_counter_diff(brickd_counter, current_match->counter);
			} else {
				uint32_t new_diff = brickd_counter_diff(brickd_counter, brickd_routing_table[i].counter);
				if(new_diff > current_diff) {
					current_match = &brickd_routing_table[i];
					current_diff = new_diff;
				}
			}
		}
	}

	if(current_match != NULL) {
		int8_t cid = current_match->cid;
		brickd_remove_route(current_match);
		return cid;
	}

	return -1;
}

void ICACHE_FLASH_ATTR brickd_disconnect(const uint8_t cid) {
	for(uint8_t i = 0; i < BRICKD_ROUTING_TABLE_SIZE; i++) {
		if(brickd_routing_table[i].cid == cid) {
			brickd_remove_route(&brickd_routing_table[i]);
		}
	}
}

void ICACHE_FLASH_ATTR brickd_print_routing_table(void) {
	bool print_something = false;
	for(uint8_t i = 0; i < BRICKD_ROUTING_TABLE_SIZE; i++) {
		if(brickd_routing_table[i].cid != -1) {
			print_something = true;
			logwohd("r %d:", i);
			logwohd(" t %d,", brickd_routing_table[i].tmp);
			logwohd(" s %d,", brickd_routing_table[i].sequence_number);
			logwohd(" id %d,", brickd_routing_table[i].cid);
			logwohd(" f %d,", brickd_routing_table[i].func_id);
			logwohd(" u %d,", brickd_routing_table[i].uid);
			logwohd(" c %d\n", brickd_routing_table[i].counter);
		}
	}
	if(print_something) {
		logwohd("bc: %d\n", brickd_counter);
	}
}

void ICACHE_FLASH_ATTR brickd_set_authentication_seed(const uint32_t seed) {
	logi("Using seed %u\n\r", seed);
	brickd_authentication_nonce = seed;
}

void ICACHE_FLASH_ATTR brickd_enable_authentication(void) {
	for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
		tfp_cons[i].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_ENABLED;
	}
}

void ICACHE_FLASH_ATTR brickd_disable_authentication(void) {
	for(uint8_t i = 0; i < TFP_MAX_CONNECTIONS; i++) {
		tfp_cons[i].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_DISABLED;
	}
}

void ICACHE_FLASH_ATTR brickd_get_authentication_nonce(const int8_t cid, const GetAuthenticationNonce *data) {
	BrickdAuthenticationState state = tfp_cons[cid].brickd_authentication_state;
	switch(state) {
		case BRICKD_AUTHENTICATION_STATE_NONCE_SEND:
			tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_ENABLED;
		case BRICKD_AUTHENTICATION_STATE_DISABLED: {
			loge("Failure during authentication nonce request: %d -> %d\n\r", state, cid);
			espconn_disconnect(tfp_cons[cid].con); // TODO: Check if disconnect callback is called
			return;
		}

		case BRICKD_AUTHENTICATION_STATE_DONE:
		case BRICKD_AUTHENTICATION_STATE_ENABLED: {
			logi("Authentication nonce requested %d\n\r", cid);
			break;
		}
	}

	GetAuthenticationNonceReturn ganr;
	ganr.header        = data->header;
	ganr.header.length = sizeof(GetAuthenticationNonceReturn);

	brickd_authentication_nonce++;

	memcpy(ganr.server_nonce, &brickd_authentication_nonce, 4);
	com_send(&ganr, sizeof(GetAuthenticationNonceReturn), cid);
	tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_NONCE_SEND;
}

void ICACHE_FLASH_ATTR brickd_authenticate(const int8_t cid, const Authenticate *data) {
	BrickdAuthenticationState state = tfp_cons[cid].brickd_authentication_state;
	switch(state) {
		case BRICKD_AUTHENTICATION_STATE_DISABLED:
		case BRICKD_AUTHENTICATION_STATE_DONE:
			tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_ENABLED;
		case BRICKD_AUTHENTICATION_STATE_ENABLED: {
			loge("Failure during authentication request: %d -> %d\n\r", state, cid);
			espconn_disconnect(tfp_cons[cid].con); // TODO: Check if disconnect callback is called
			return;
		}

		case BRICKD_AUTHENTICATION_STATE_NONCE_SEND: {
			loge("Authentication requested %d\n\r", cid);
			break;
		}
	}

	uint32_t nonces[2];
	uint8_t digest[SHA1_DIGEST_LENGTH];

	memcpy(&nonces[0], &brickd_authentication_nonce, 4);
	memcpy(&nonces[1], data->client_nonce, 4);

	hmac_sha1((uint8_t *)configuration_current.general_authentication_secret,
	          strnlen(configuration_current.general_authentication_secret, CONFIGURATION_SECRET_MAX_LENGTH),
	          (uint8_t *)nonces,
	          sizeof(nonces),
	          digest);

	if(memcmp(data->digest, digest, SHA1_DIGEST_LENGTH) != 0) {
		tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_ENABLED;
		espconn_disconnect(tfp_cons[cid].con); // TODO: Check if disconnect callback is called
		return;
	} else {
		tfp_cons[cid].brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_DONE;
	}

	com_return_setter(cid, data);
}

bool ICACHE_FLASH_ATTR brickd_check_auth(const MessageHeader *header, const int8_t cid) {
	if(cid < 0) {
		return true;
	}

	if(tfp_cons[cid].brickd_authentication_state == BRICKD_AUTHENTICATION_STATE_DISABLED ||
	   tfp_cons[cid].brickd_authentication_state == BRICKD_AUTHENTICATION_STATE_DONE) {
		return true;
	}

	if(header && header->uid == 1) {
		return true;
	}

	return false;
}
