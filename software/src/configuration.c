/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * configuration.c: WIFI Extension configuration handling
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

#include "configuration.h"

#include "espmissingincludes.h"
#include "eeprom.h"
#include "ip_addr.h"
#include "espconn.h"
#include "osapi.h"
#include "user_interface.h"
#include "pearson_hash.h"
#include "communication.h"

// Mesh related.
#include "mesh.h"
#include "tfp_mesh_connection.h"

const Configuration configuration_default = {
	// Configuration info
	.conf_checksum = 0,
	.conf_version = CONFIGURATION_EXPECTED_VERSION,
	.conf_length = sizeof(Configuration),

	// General configuration
	.general_port = 4223,
	.general_websocket_port = 4280,
	.general_website_port = 80,
	.general_phy_mode = 1,
	.general_sleep_mode = 0, // Currently unused
	/*
	 * The field "general_website" is currently unused.
	 * The web interface is currently enabled/disabled
	 * based on the port specified for it.
	 */
	.general_website = 0,
	.general_authentication_secret = "",

	// Client configuration
	.client_enable = 1,
	.client_ssid = "Tinkerforge WLAN",
	.client_ip = {0, 0, 0, 0},
	.client_subnet_mask = {0, 0, 0, 0},
	.client_gateway = {0, 0, 0, 0},
	.client_mac_address = {0, 0, 0, 0, 0, 0},
	.client_bssid = {0, 0, 0, 0, 0, 0},
	.client_hostname = "wifi-extension-v2",
	.client_password = "25842149320894763607",

	// AP configuration
	.ap_enable = 1,
	.ap_ssid = "Wifi Extension 2.0 Access Point",
	.ap_ip = {0, 0, 0, 0},
	.ap_subnet_mask = {0, 0, 0, 0},
	.ap_gateway = {0, 0, 0, 0},
	.ap_encryption = 4,
	.ap_hidden = false,
	.ap_mac_address = {0, 0, 0, 0, 0, 0},
	.ap_channel = 1,
	.ap_hostname = "\0", // Currently unused
	.ap_password = "password"
};

Configuration configuration_current;

uint8_t ICACHE_FLASH_ATTR configuration_calculate_checksum(void) {
	uint8_t checksum = 0;

	uint8_t *conf_array = (uint8_t*)&configuration_current;
	for(uint16_t i = 1; i < sizeof(Configuration); i++) {
		PEARSON(checksum, conf_array[i]);
	}

	return checksum;
}

bool ICACHE_FLASH_ATTR configuration_check_checksum(void) {
	const uint8_t checksum = configuration_calculate_checksum();
	return checksum == configuration_current.conf_checksum;
}

void ICACHE_FLASH_ATTR configuration_use_default(void) {
	os_memcpy(&configuration_current, &configuration_default, sizeof(Configuration));
}

void ICACHE_FLASH_ATTR configuration_load_from_eeprom(void) {
	if(!((eeprom_read(CONFIGURATION_ADDRESS, (uint8_t*)&configuration_current, sizeof(Configuration))) &&
	     (configuration_current.conf_version == CONFIGURATION_EXPECTED_VERSION) &&
	     (configuration_check_checksum()))) {

		// In case of any kind of error we use the default configuration
		// and save it to the eeprom for the next restart.
		configuration_use_default();
		configuration_save_to_eeprom();
	}
}

uint8_t ICACHE_FLASH_ATTR configuration_save_to_eeprom(void) {
	configuration_current.conf_checksum = configuration_calculate_checksum();
	if(eeprom_write(CONFIGURATION_ADDRESS, (uint8_t*)&configuration_current, sizeof(Configuration))) {
		return 0;
	}

	return 1;
}

bool ICACHE_FLASH_ATTR configuration_check_array_null(const uint8_t *array, const uint8_t length) {
	for(uint8_t i = 0; i < length; i++) {
		if(array[i] != 0) {
			return false;
		}
	}

	return true;
}

void ICACHE_FLASH_ATTR configuration_apply_client(void) {
	struct station_config conf;
	os_memcpy(conf.ssid, configuration_current.client_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(conf.password, configuration_current.client_password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	os_memcpy(conf.bssid, configuration_current.client_bssid, 6);
	if(configuration_check_array_null(configuration_current.client_bssid, 6)) {
		conf.bssid_set = 0;
	}
	else{
		conf.bssid_set = 1;
	}
	wifi_station_set_config_current(&conf);

	if(configuration_check_array_null(configuration_current.client_ip, 4)) {
		wifi_station_dhcpc_start();
	} else {
		wifi_station_dhcpc_stop();
		struct ip_info info;
		IP4_ADDR(&info.ip, configuration_current.client_ip[0],
		                   configuration_current.client_ip[1],
		                   configuration_current.client_ip[2],
		                   configuration_current.client_ip[3]);
		IP4_ADDR(&info.gw, configuration_current.client_gateway[0],
		                   configuration_current.client_gateway[1],
		                   configuration_current.client_gateway[2],
		                   configuration_current.client_gateway[3]);
		IP4_ADDR(&info.netmask, configuration_current.client_subnet_mask[0],
		                        configuration_current.client_subnet_mask[1],
		                        configuration_current.client_subnet_mask[2],
		                        configuration_current.client_subnet_mask[3]);
		wifi_set_ip_info(STATION_IF, &info);
	}

	if(!configuration_check_array_null(configuration_current.client_mac_address, 6)) {
		wifi_set_macaddr(STATION_IF, configuration_current.client_mac_address);
	}

	char client_hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH + 1] = {0}; // +1 for NUL-terminator
	os_memcpy(client_hostname, configuration_current.client_hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);

	wifi_station_set_hostname(client_hostname);
	wifi_station_set_reconnect_policy(true); // Always call from user_init
}

void ICACHE_FLASH_ATTR configuration_apply_ap(void) {
	struct softap_config conf;
	os_memcpy(conf.ssid, configuration_current.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(conf.password, configuration_current.ap_password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	conf.ssid_len = strnlen(configuration_current.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	conf.channel = configuration_current.ap_channel;

	if(configuration_current.ap_encryption == 1){
		// WEP is not supported. Switches to default WPA/WPA2.
		conf.authmode = 4;
		configuration_current.ap_encryption = 4;
	}
	else{
		conf.authmode = configuration_current.ap_encryption;
	}

	conf.ssid_hidden = configuration_current.ap_hidden;
	conf.max_connection = 4; // Max number here is 4
	conf.beacon_interval = 100;
	wifi_softap_set_config_current(&conf);

	if(configuration_check_array_null(configuration_current.ap_ip, 4)) {
		wifi_softap_dhcps_stop();
		struct ip_info info;
		IP4_ADDR(&info.ip, 192, 168, 42, 1);
		IP4_ADDR(&info.gw, 192, 168, 42, 1);
		IP4_ADDR(&info.netmask, 255, 255, 255, 0);
		wifi_set_ip_info(SOFTAP_IF, &info);

		struct dhcps_lease dhcp_lease;
		IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 42, 100);
		IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 42, 105);
		wifi_softap_set_dhcps_lease(&dhcp_lease);

		uint8 mode = 1;
		wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode);

		wifi_softap_dhcps_start();
	} else {
		wifi_softap_dhcps_stop();
		struct ip_info info;
		IP4_ADDR(&info.ip, configuration_current.ap_ip[0],
		                   configuration_current.ap_ip[1],
		                   configuration_current.ap_ip[2],
		                   configuration_current.ap_ip[3]);
		IP4_ADDR(&info.gw, configuration_current.ap_gateway[0],
		                   configuration_current.ap_gateway[1],
		                   configuration_current.ap_gateway[2],
		                   configuration_current.ap_gateway[3]);
		IP4_ADDR(&info.netmask, configuration_current.ap_subnet_mask[0],
		                        configuration_current.ap_subnet_mask[1],
		                        configuration_current.ap_subnet_mask[2],
		                        configuration_current.ap_subnet_mask[3]);
		wifi_set_ip_info(SOFTAP_IF, &info);
	}

	if(!configuration_check_array_null(configuration_current.ap_mac_address, 6)) {
		wifi_set_macaddr(SOFTAP_IF, configuration_current.ap_mac_address);
	}

	/* TODO: MDNS does not seem to work currently.
	 *       Check again in later SDK version
		struct mdns_info dns;
		struct ip_info sta_ip;
		wifi_get_ip_info(STATION_IF, &sta_ip);
		char host_name[] = "host";
		char server_name[] = "server";
		dns.host_name = (char*)&host_name;
		dns.server_name = (char*)&server_name;
		dns.txt_data[0] = (char*)NULL;
		dns.ipAddr = sta_ip.ip.addr;
		dns.server_port = 80;
		espconn_mdns_init(&dns);
	*/
}

void ICACHE_FLASH_ATTR configuration_apply_tf_mesh(void) {
	// For mesh mode we operate on station mode and configure the station interface.
	os_printf("\n[+]MSH:Applying mesh configuration\n");

	struct ip_info info_ip;
	struct station_config config_st;
	char hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH + 1] = {0};

	wifi_set_sleep_type(NONE_SLEEP_T);

	/*
	 * Even though in mesh mode enabling and configuring only station interface is
	 * enough, we enable STATIONAP mode to apply AP mode settings like AP MAC address
	 * if user provides one.
	 */
	wifi_set_opmode_current(STATIONAP_MODE);

	os_bzero(&config_st, sizeof(config_st));
	os_bzero(&info_ip, sizeof(info_ip));

	os_memcpy(config_st.ssid, TFP_MESH_ROUTER_SSID, os_strlen(TFP_MESH_ROUTER_SSID));
	os_memcpy(config_st.password, TFP_MESH_ROUTER_SSID_PASSWORD,
		os_strlen(TFP_MESH_ROUTER_SSID_PASSWORD));
	os_memcpy(config_st.bssid, TFP_MESH_ROUTER_BSSID, sizeof(config_st.bssid));

	if(configuration_check_array_null(configuration_current.client_bssid, 6)) {
		config_st.bssid_set = 0;
	}
	else{
		config_st.bssid_set = 1;
	}

	// Set MAC address of station and AP interfaces if there are user defined ones.
	if(!configuration_check_array_null(configuration_current.client_mac_address, 6)) {
		wifi_set_macaddr(STATION_IF, configuration_current.client_mac_address);
	}

	if(!configuration_check_array_null(configuration_current.ap_mac_address, 6)) {
		wifi_set_macaddr(SOFTAP_IF, configuration_current.ap_mac_address);
	}

	/*
	 * If the mesh router doesn't have DHCP enabled then static IP must be configured on
	 * the station interface.
	 */
	if(configuration_check_array_null(configuration_current.client_ip, 4)) {
		wifi_station_dhcpc_start();
	}
	else {
		wifi_station_dhcpc_stop();

		IP4_ADDR(&info_ip.ip, configuration_current.client_ip[0],
			configuration_current.client_ip[1],
			configuration_current.client_ip[2],
			configuration_current.client_ip[3]);
		IP4_ADDR(&info_ip.gw, configuration_current.client_gateway[0],
			configuration_current.client_gateway[1],
			configuration_current.client_gateway[2],
			configuration_current.client_gateway[3]);
		IP4_ADDR(&info_ip.netmask, configuration_current.client_subnet_mask[0],
			configuration_current.client_subnet_mask[1],
			configuration_current.client_subnet_mask[2],
			configuration_current.client_subnet_mask[3]);

		wifi_set_ip_info(STATION_IF, &info_ip);
	}

	os_memcpy(hostname, configuration_current.client_hostname,
		CONFIGURATION_HOSTNAME_MAX_LENGTH);

	wifi_station_set_hostname(hostname);

	// Apply station interface configuration.
	wifi_station_set_config_current(&config_st);

	// Setup mesh network parameters.
	espconn_mesh_set_router(&config_st);

	espconn_mesh_encrypt_init(TFP_MESH_NODE_SSID_AUTHENTICATION,
	  TFP_MESH_NODE_SSID_PASSWORD,
	  os_strlen(TFP_MESH_NODE_SSID_PASSWORD));

	espconn_mesh_set_max_hops(TFP_MESH_MAX_HOP);

	espconn_mesh_set_ssid_prefix(TFP_MESH_NODE_SSID_PREFIX,
	  os_strlen(TFP_MESH_NODE_SSID_PREFIX));

	espconn_mesh_group_id_init((uint8_t *)TFP_MESH_GROUP_ID,
	sizeof(TFP_MESH_GROUP_ID));

	// Used in MESH_ONLINE mode.
	espconn_mesh_server_init((struct ip_addr *)TFP_MESH_SERVER_IP,
	TFP_MESH_SERVER_PORT);

	// Callback for the event when a new child joins a node.
	espconn_mesh_regist_usr_cb(cb_tfp_mesh_new_node);
}

void ICACHE_FLASH_ATTR configuration_apply_during_init(void) {
	// TODO: Currently not implemented in brickv
	wifi_set_sleep_type(NONE_SLEEP_T);

	/*
	 * Ultimately this will be detected from the loaded configuration like how it
	 * is done currently for STATION, AP and STATION+AP mode.
	 */
	#if(MESH_ENABLED == 1)
		configuration_apply_tf_mesh();
	#else
		if(configuration_current.client_enable && configuration_current.ap_enable) {
			wifi_set_opmode_current(STATIONAP_MODE);
		} else if(configuration_current.client_enable) {
			wifi_set_opmode_current(STATION_MODE);
		} else if(configuration_current.ap_enable) {
			wifi_set_opmode_current(SOFTAP_MODE);
		} else {
			wifi_set_opmode_current(NULL_MODE);
		}

		wifi_set_phy_mode(configuration_current.general_phy_mode+1);

		// Client
		if(configuration_current.client_enable) {
			configuration_apply_client();
		}

		// Access Point
		if(configuration_current.ap_enable) {
			configuration_apply_ap();
		}

		// Do always call during user_init, otherwise it will only be effective after restart!
		wifi_station_set_auto_connect(1);
	#endif
}

void ICACHE_FLASH_ATTR configuration_apply_post_init(void) {
	// Station connect can only be called after user_init
	wifi_station_connect();
}
