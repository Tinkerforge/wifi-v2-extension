/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * configuration.h: WIFI Extension configuration handling
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "config.h"
#include "c_types.h"
#include "mesh.h"

#define CONFIGURATION_EXPECTED_VERSION 2

#define CONFIGURATION_ADDRESS 32

#define CONFIGURATION_SECRET_MAX_LENGTH 64
#define CONFIGURATION_HOSTNAME_MAX_LENGTH 32
#define CONFIGURATION_PASSWORD_MAX_LENGTH 64
#define CONFIGURATION_SSID_MAX_LENGTH 32

typedef struct {
	// Configuration info
	uint8_t  conf_checksum;
	uint8_t  conf_version;
	uint16_t conf_length;

	// General configuration
	uint16_t general_port;
	uint16_t general_websocket_port;
	uint16_t general_website_port;
	uint8_t  general_phy_mode;
	uint8_t  general_sleep_mode;
	uint8_t  general_website;
	char     general_authentication_secret[CONFIGURATION_SECRET_MAX_LENGTH]; // might not be NULL-terminated

	// Client configuration
	bool     client_enable;
	char     client_ssid[CONFIGURATION_SSID_MAX_LENGTH]; // might not be NULL-terminated
	uint8_t  client_ip[4];
	uint8_t  client_subnet_mask[4];
	uint8_t  client_gateway[4];
	uint8_t  client_mac_address[6];
	uint8_t  client_bssid[6];
	char     client_hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH]; // might not be NULL-terminated
	char     client_password[CONFIGURATION_PASSWORD_MAX_LENGTH]; // might not be NULL-terminated

	// AP configuration
	bool     ap_enable;
	char     ap_ssid[CONFIGURATION_SSID_MAX_LENGTH]; // might not be NULL-terminated
	uint8_t  ap_ip[4];
	uint8_t  ap_subnet_mask[4];
	uint8_t  ap_gateway[4];
	uint8_t  ap_encryption;
	bool     ap_hidden;
	uint8_t  ap_mac_address[6];
	uint8_t  ap_channel;
	char     ap_hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH]; // might not be NULL-terminated
	char     ap_password[CONFIGURATION_PASSWORD_MAX_LENGTH]; // might not be NULL-terminated

	// Mesh configuration
	bool     mesh_enable;
	char     mesh_router_ssid[CONFIGURATION_SSID_MAX_LENGTH]; // might not be NULL-terminated
	char     mesh_router_password[CONFIGURATION_PASSWORD_MAX_LENGTH]; // might not be NULL-terminated
	uint8_t  mesh_router_ip[4];
	uint8_t  mesh_router_subnet_mask[4];
	uint8_t  mesh_router_gateway[4];
	uint8_t  mesh_router_bssid[6];
	char		 mesh_ssid_prefix[CONFIGURATION_SSID_MAX_LENGTH / 2];
	char     mesh_password[CONFIGURATION_PASSWORD_MAX_LENGTH]; // might not be NULL-terminated
	uint8_t  mesh_group_id[ESP_MESH_GROUP_ID_LEN];
	char 		 mesh_gateway_hostname[32]; // Currently unused.
	uint8_t  mesh_gateway_ip[4];
	uint16_t mesh_gateway_port;
} Configuration;

void ICACHE_FLASH_ATTR configuration_use_default(void);
void ICACHE_FLASH_ATTR configuration_load_from_eeprom(void);
uint8_t ICACHE_FLASH_ATTR configuration_save_to_eeprom(void);
void ICACHE_FLASH_ATTR configuration_apply_during_init(void);
void ICACHE_FLASH_ATTR configuration_apply_post_init(void);
void ICACHE_FLASH_ATTR configuration_init(void);
void ICACHE_FLASH_ATTR configuration_apply_tf_mesh(void);

#endif
