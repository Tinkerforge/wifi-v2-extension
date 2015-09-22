/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
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

#include "configuration.h"

#include "espmissingincludes.h"
#include "eeprom.h"
#include "osapi.h"
#include "user_interface.h"

const Configuration configuration_default = {
	// Configuration info
	.conf_version = CONFIGURATION_EXPECTED_VERSION,
	.conf_length = sizeof(Configuration),
	.conf_checksum = 0,

	// General configuration
	.general_port = 4223,
	.general_websocket_port = 4280,
	.general_website_port = 80,
	.general_phy_mode = 1,
	.general_sleep_mode = 0,
	.general_website = true,
	.general_authentication_secret = {0},

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
	.ap_enable = 0,
	.ap_ssid = "Wifi Extension 2.0 Access Point",
	.ap_ip = {0, 0, 0, 0},
	.ap_subnet_mask = {0, 0, 0, 0},
	.ap_gateway = {0, 0, 0, 0},
	.ap_encryption = 4,
	.ap_hidden = false,
	.ap_mac_address = {0, 0, 0, 0, 0, 0},
	.ap_password = "password"
};

Configuration configuration_current;

uint8_t ICACHE_FLASH_ATTR configuration_array_length(const uint8_t *data, const uint8_t max_length) {
	for(uint8_t i = 0; i < max_length; i++) {
		if(data[i] == '\0') {
			return i+1;
		}
	}

	return max_length;
}

bool ICACHE_FLASH_ATTR configuration_check_checksum(void) {
	// TODO: Implement me
	return true;
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
	// TODO: Calculate and save checksum

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
	os_memcpy(&conf.ssid,
			  configuration_current.client_ssid,
			  configuration_array_length(configuration_current.client_ssid, CONFIGURATION_SSID_MAX_LENGTH));
	os_memcpy(&conf.password,
			  configuration_current.client_password,
			  configuration_array_length(configuration_current.client_password, CONFIGURATION_PASSWORD_MAX_LENGTH));
	os_memcpy(conf.bssid, configuration_current.client_bssid, 6);
	if(configuration_check_array_null(configuration_current.client_bssid, 6)) {
		conf.bssid_set = 0;
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


	wifi_station_set_hostname(configuration_current.client_hostname);


	wifi_station_set_reconnect_policy(true);
}

void ICACHE_FLASH_ATTR configuration_apply_ap(void) {
	struct softap_config conf;
	os_memcpy(&conf.ssid,
			  configuration_current.ap_ssid,
			  configuration_array_length(configuration_current.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH));
	os_memcpy(&conf.password,
			  configuration_current.ap_password,
			  configuration_array_length(configuration_current.ap_password, CONFIGURATION_PASSWORD_MAX_LENGTH));
	conf.ssid_len = configuration_array_length(configuration_current.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH)-1;
	conf.channel = 1; // TODO: Should we make this configurable?
	conf.authmode = configuration_current.ap_encryption;
	conf.ssid_hidden = configuration_current.ap_hidden;
	conf.max_connection = 4; // TODO: Should we make this configurable?
	conf.beacon_interval = 100; // TODO: Should we make this configurable?
	wifi_softap_set_config_current(&conf);

	if(configuration_check_array_null(configuration_current.ap_ip, 4)) {
		wifi_softap_dhcps_stop();
		struct ip_info info;
		IP4_ADDR(&info.ip, 192, 168, 5, 1);
		IP4_ADDR(&info.gw, 192, 168, 5, 1);
		IP4_ADDR(&info.netmask, 255, 255, 255, 0);
		wifi_set_ip_info(SOFTAP_IF, &info);

		struct dhcps_lease dhcp_lease;
		IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 5, 100);
		IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 5, 105);
		wifi_softap_set_dhcps_lease(&dhcp_lease);

		uint8 mode = 1;
		wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode);

		wifi_softap_dhcps_start();
	} else {
		wifi_softap_dhcps_stop();
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
		wifi_set_ip_info(SOFTAP_IF, &info);
	}

	if(!configuration_check_array_null(configuration_current.ap_mac_address, 6)) {
		wifi_set_macaddr(SOFTAP_IF, configuration_current.ap_mac_address);
	}
}

void ICACHE_FLASH_ATTR configuration_apply(void) {
	// TODO: Currently not implemented in brickv
	wifi_set_sleep_type(NONE_SLEEP_T);

	// General
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


	wifi_station_set_auto_connect(1);

	wifi_station_connect();
}
