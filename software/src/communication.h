/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * communication.h: Handling of communication between PC and WIFI Extension
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

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "c_types.h"
#include "configuration.h"

#define COM_OUT_RINGBUFFER_LENGTH 512

#define MESSAGE_HEADER_LENGTH_POSITION 4

#define MESSAGE_ERROR_CODE_OK 0
#define MESSAGE_ERROR_CODE_INVALID_PARAMETER 1
#define MESSAGE_ERROR_CODE_NOT_SUPPORTED 2


typedef struct {
	uint32_t uid;
	uint8_t length;
	uint8_t fid;
	uint8_t other_options:2,
	        authentication:1,
	        return_expected:1,
			sequence_num:4;
	uint8_t future_use:6,
	        error:2;
} __attribute__((__packed__)) MessageHeader;

void ICACHE_FLASH_ATTR com_init(void);
void ICACHE_FLASH_ATTR com_poll(void);
void ICACHE_FLASH_ATTR com_send(const void *data, const uint8_t length, const int8_t cid);
bool ICACHE_FLASH_ATTR com_handle_message(const uint8_t *data, const uint8_t length, const int8_t cid);
void ICACHE_FLASH_ATTR com_return_error(const void *data, const uint8_t ret_length, const uint8_t error_code, const int8_t cid);
void ICACHE_FLASH_ATTR com_return_setter(const int8_t cid, const void *data);

#define FID_SET_WIFI2_AUTHENTICATION_SECRET 82
#define FID_GET_WIFI2_AUTHENTICATION_SECRET 83
#define FID_SET_WIFI2_CONFIGURATION 84
#define FID_GET_WIFI2_CONFIGURATION 85
#define FID_GET_WIFI2_STATUS 86
#define FID_SET_WIFI2_CLIENT_CONFIGURATION 87
#define FID_GET_WIFI2_CLIENT_CONFIGURATION 88
#define FID_SET_WIFI2_CLIENT_HOSTNAME 89
#define FID_GET_WIFI2_CLIENT_HOSTNAME 90
#define FID_SET_WIFI2_CLIENT_PASSWORD 91
#define FID_GET_WIFI2_CLIENT_PASSWORD 92
#define FID_SET_WIFI2_AP_CONFIGURATION 93
#define FID_GET_WIFI2_AP_CONFIGURATION 94
#define FID_SET_WIFI2_AP_PASSWORD 95
#define FID_GET_WIFI2_AP_PASSWORD 96
#define FID_SAVE_WIFI2_CONFIGURATION 97
#define FID_GET_WIFI2_FIRMWARE_VERSION 98
#define FID_ENABLE_WIFI2_STATUS_LED 99
#define FID_DISABLE_WIFI2_STATUS_LED 100
#define FID_IS_WIFI2_STATUS_LED_ENABLED 101
#define FID_SET_WIFI2_MESH_CONFIGURATION 102
#define FID_GET_WIFI2_MESH_CONFIGURATION 103
#define FID_SET_WIFI2_MESH_ROUTER_SSID 104
#define FID_GET_WIFI2_MESH_ROUTER_SSID 105
#define FID_SET_WIFI2_MESH_ROUTER_PASSWORD 106
#define FID_GET_WIFI2_MESH_ROUTER_PASSWORD 107
#define FID_GET_WIFI2_MESH_COMMON_STATUS 108
#define FID_GET_WIFI2_MESH_CLIENT_STATUS 109
#define FID_GET_WIFI2_MESH_AP_STATUS 110

typedef struct {
	MessageHeader header;
	char secret[64];
} __attribute__((__packed__)) SetWifi2AuthenticationSecret;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2AuthenticationSecret;

typedef struct {
	MessageHeader header;
	char secret[64];
} __attribute__((__packed__)) GetWifi2AuthenticationSecretReturn;

typedef struct {
	MessageHeader header;
	uint16_t port;
	uint16_t websocket_port;
	uint16_t website_port;
	uint8_t phy_mode;
	uint8_t sleep_mode;
	uint8_t website;
} __attribute__((__packed__)) SetWifi2Configuration;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2Configuration;

typedef struct {
	MessageHeader header;
	uint16_t port;
	uint16_t websocket_port;
	uint16_t website_port;
	uint8_t phy_mode;
	uint8_t sleep_mode;
	uint8_t website;
} __attribute__((__packed__)) GetWifi2ConfigurationReturn;

typedef struct {
	MessageHeader header;
	bool     mesh_enable;
	uint8_t  mesh_root_ip[4];
	uint8_t  mesh_root_subnet_mask[4];
	uint8_t  mesh_root_gateway[4];
	uint8_t  mesh_router_bssid[6];
	uint8_t  mesh_group_id[6];
	char     mesh_group_ssid_prefix[CONFIGURATION_SSID_MAX_LENGTH / 2];
	uint8_t  mesh_gateway_ip[4];
	uint16_t mesh_gateway_port;
} __attribute__((__packed__)) SetWifi2MeshConfiguration;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2MeshConfiguration;

typedef struct {
	MessageHeader header;
	bool     mesh_enable;
	uint8_t  mesh_root_ip[4];
	uint8_t  mesh_root_subnet_mask[4];
	uint8_t  mesh_root_gateway[4];
	uint8_t  mesh_router_bssid[6];
	uint8_t  mesh_group_id[6];
	char     mesh_group_ssid_prefix[CONFIGURATION_SSID_MAX_LENGTH / 2];
	uint8_t  mesh_gateway_ip[4];
	uint16_t mesh_gateway_port;
} __attribute__((__packed__)) GetWifi2MeshConfigurationReturn;

typedef struct {
	MessageHeader header;
	// Espressif mesh library supports router SSID with maximum length of 31 characters.
	char mesh_router_ssid[CONFIGURATION_SSID_MAX_LENGTH];
} __attribute__((__packed__)) SetWifi2MeshRouterSSID;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2MeshRouterSSID;

typedef struct {
	MessageHeader header;
	// Espressif mesh library supports router SSID with maximum length of 31 characters.
	char mesh_router_ssid[CONFIGURATION_SSID_MAX_LENGTH];
} __attribute__((__packed__)) GetWifi2MeshRouterSSIDReturn;

typedef struct {
	MessageHeader header;
	char mesh_router_password[CONFIGURATION_PASSWORD_MAX_LENGTH];
} __attribute__((__packed__)) SetWifi2MeshRouterPassword;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2MeshRouterPassword;

typedef struct {
	MessageHeader header;
	char mesh_router_password[CONFIGURATION_PASSWORD_MAX_LENGTH];
} __attribute__((__packed__)) GetWifi2MeshRouterPasswordReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2Status;

typedef struct {
	MessageHeader header;
	bool     client_enabled;
	uint8_t  client_status;
	uint8_t  client_ip[4];
	uint8_t  client_subnet_mask[4];
	uint8_t  client_gateway[4];
	uint8_t  client_mac_address[6];
	uint32_t client_rx_count;
	uint32_t client_tx_count;
	int8_t   client_rssi;
	bool     ap_enabled;
	uint8_t  ap_ip[4];
	uint8_t  ap_subnet_mask[4];
	uint8_t  ap_gateway[4];
	uint8_t  ap_mac_address[6];
	uint32_t ap_rx_count;
	uint32_t ap_tx_count;
	uint8_t  ap_connected_count;
} __attribute__((__packed__)) GetWifi2StatusReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2MeshCommonStatus;

typedef struct {
	MessageHeader header;
	uint8_t status;
	bool is_root_node;
	bool is_root_candidate;
	uint16_t connected_nodes;
	uint32_t rx_count;
	uint32_t tx_count;
} __attribute__((__packed__)) GetWifi2MeshCommonStatusReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2MeshClientStatus;

typedef struct {
	MessageHeader header;
	char hostname[32];
	uint8_t ip[4];
	uint8_t sub[4];
	uint8_t gw[4];
	uint8_t mac[6];
} __attribute__((__packed__)) GetWifi2MeshClientStatusReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2MeshAPStatus;

typedef struct {
	MessageHeader header;
	char ssid[32];
	uint8_t ip[4];
	uint8_t sub[4];
	uint8_t gw[4];
	uint8_t mac[6];
} __attribute__((__packed__)) GetWifi2MeshAPStatusReturn;

typedef struct {
	MessageHeader header;
	bool enable;
	char ssid[32];
	uint8_t ip[4];
	uint8_t subnet_mask[4];
	uint8_t gateway[4];
	uint8_t mac_address[6];
	uint8_t bssid[6];
} __attribute__((__packed__)) SetWifi2ClientConfiguration;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2ClientConfiguration;

typedef struct {
	MessageHeader header;
	bool enable;
	char ssid[32];
	uint8_t ip[4];
	uint8_t subnet_mask[4];
	uint8_t gateway[4];
	uint8_t mac_address[6];
	uint8_t bssid[6];
} __attribute__((__packed__)) GetWifi2ClientConfigurationReturn;

typedef struct {
	MessageHeader header;
	char hostname[32];
} __attribute__((__packed__)) SetWifi2ClientHostname;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2ClientHostname;

typedef struct {
	MessageHeader header;
	char hostname[32];
} __attribute__((__packed__)) GetWifi2ClientHostnameReturn;

typedef struct {
	MessageHeader header;
	char password[64];
} __attribute__((__packed__)) SetWifi2ClientPassword;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2ClientPassword;

typedef struct {
	MessageHeader header;
	char password[64];
} __attribute__((__packed__)) GetWifi2ClientPasswordReturn;

typedef struct {
	MessageHeader header;
	bool enable;
	char ssid[32];
	uint8_t ip[4];
	uint8_t subnet_mask[4];
	uint8_t gateway[4];
	uint8_t encryption;
	bool hidden;
	uint8_t channel;
	uint8_t mac_address[6];
} __attribute__((__packed__)) SetWifi2APConfiguration;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2APConfiguration;

typedef struct {
	MessageHeader header;
	bool enable;
	char ssid[32];
	uint8_t ip[4];
	uint8_t subnet_mask[4];
	uint8_t gateway[4];
	uint8_t encryption;
	bool hidden;
	uint8_t channel;
	uint8_t mac_address[6];
} __attribute__((__packed__)) GetWifi2APConfigurationReturn;

typedef struct {
	MessageHeader header;
	char password[64];
} __attribute__((__packed__)) SetWifi2APPassword;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2APPassword;

typedef struct {
	MessageHeader header;
	char password[64];
} __attribute__((__packed__)) GetWifi2APPasswordReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) SaveWifi2Configuration;

typedef struct {
	MessageHeader header;
	uint8_t result;
} __attribute__((__packed__)) SaveWifi2ConfigurationReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) GetWifi2FirmwareVersion;

typedef struct {
	MessageHeader header;
	uint8_t version_fw[3];
} __attribute__((__packed__)) GetWifi2FirmwareVersionReturn;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) EnableWifi2StatusLED;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) DisableWifi2StatusLED;

typedef struct {
	MessageHeader header;
} __attribute__((__packed__)) IsWifi2StatusLEDEnabled;

typedef struct {
	MessageHeader header;
	bool enabled;
} __attribute__((__packed__)) IsWifi2StatusLEDEnabledReturn;

void ICACHE_FLASH_ATTR set_wifi2_authentication_secret(const int8_t cid, const SetWifi2AuthenticationSecret *data);
void ICACHE_FLASH_ATTR get_wifi2_authentication_secret(const int8_t cid, const GetWifi2AuthenticationSecret *data);
void ICACHE_FLASH_ATTR set_wifi2_configuration(const int8_t cid, const SetWifi2Configuration *data);
void ICACHE_FLASH_ATTR get_wifi2_configuration(const int8_t cid, const GetWifi2Configuration *data);
void ICACHE_FLASH_ATTR get_wifi2_status(const int8_t cid, const GetWifi2Status *data);
void ICACHE_FLASH_ATTR set_wifi2_client_configuration(const int8_t cid, const SetWifi2ClientConfiguration *data);
void ICACHE_FLASH_ATTR get_wifi2_client_configuration(const int8_t cid, const GetWifi2ClientConfiguration *data);
void ICACHE_FLASH_ATTR set_wifi2_client_hostname(const int8_t cid, const SetWifi2ClientHostname *data);
void ICACHE_FLASH_ATTR get_wifi2_client_hostname(const int8_t cid, const GetWifi2ClientHostname *data);
void ICACHE_FLASH_ATTR set_wifi2_client_password(const int8_t cid, const SetWifi2ClientPassword *data);
void ICACHE_FLASH_ATTR get_wifi2_client_password(const int8_t cid, const GetWifi2ClientPassword *data);
void ICACHE_FLASH_ATTR set_wifi2_ap_configuration(const int8_t cid, const SetWifi2APConfiguration *data);
void ICACHE_FLASH_ATTR get_wifi2_ap_configuration(const int8_t cid, const GetWifi2APConfiguration *data);
void ICACHE_FLASH_ATTR set_wifi2_ap_password(const int8_t cid, const SetWifi2APPassword *data);
void ICACHE_FLASH_ATTR get_wifi2_ap_password(const int8_t cid, const GetWifi2APPassword *data);
void ICACHE_FLASH_ATTR save_wifi2_configuration(const int8_t cid, const SaveWifi2Configuration *data);
void ICACHE_FLASH_ATTR get_wifi2_firmware_version(const int8_t cid, const GetWifi2FirmwareVersion *data);
void ICACHE_FLASH_ATTR enable_wifi2_status_led(const int8_t cid, const EnableWifi2StatusLED *data);
void ICACHE_FLASH_ATTR disable_wifi2_status_led(const int8_t cid, const DisableWifi2StatusLED *data);
void ICACHE_FLASH_ATTR is_wifi2_status_led_enabled(const int8_t cid, const IsWifi2StatusLEDEnabled *data);
void ICACHE_FLASH_ATTR set_wifi2_mesh_configuration(const int8_t cid, const SetWifi2MeshConfiguration *data);
void ICACHE_FLASH_ATTR get_wifi2_mesh_configuration(const int8_t cid, const GetWifi2MeshConfiguration *data);
void ICACHE_FLASH_ATTR set_wifi2_mesh_router_ssid(const int8_t cid, const SetWifi2MeshRouterSSID *data);
void ICACHE_FLASH_ATTR get_wifi2_mesh_router_ssid(const int8_t cid, const GetWifi2MeshRouterSSID *data);
void ICACHE_FLASH_ATTR set_wifi2_mesh_router_password(const int8_t cid, const SetWifi2MeshRouterPassword *data);
void ICACHE_FLASH_ATTR get_wifi2_mesh_router_password(const int8_t cid, const GetWifi2MeshRouterPassword *data);
void ICACHE_FLASH_ATTR get_wifi2_mesh_common_status(const int8_t cid, const GetWifi2MeshCommonStatus *data);
void ICACHE_FLASH_ATTR get_wifi2_mesh_client_status(const int8_t cid, const GetWifi2MeshClientStatus *data);
void ICACHE_FLASH_ATTR get_wifi2_mesh_ap_status(const int8_t cid, const GetWifi2MeshAPStatus *data);

#endif
