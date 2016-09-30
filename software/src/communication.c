/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * communication.c: Handling of communication between PC and WIFI Extension
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

#include "communication.h"

#include "tfp_connection.h"
#include "configuration.h"
#include "brickd.h"
#include "osapi.h"
#include "logging.h"
#include "user_interface.h"
#include "ip_addr.h"
#include "espconn.h"
#include "gpio.h"

bool wifi2_status_led_enabled = true;
extern Configuration configuration_current;

// We keep this global, some of the information is set dynamically by callbacks
GetWifi2StatusReturn gw2sr = {
	.client_enabled = false,
	.client_status = 0,
	.client_ip = {0, 0, 0, 0},
	.client_subnet_mask = {0, 0, 0, 0},
	.client_gateway = {0, 0, 0, 0},
	.client_mac_address = {0, 0, 0, 0, 0, 0},
	.client_rx_count = 0,
	.client_tx_count = 0,
	.client_rssi = 0,
	.ap_enabled = false,
	.ap_ip = {0, 0, 0, 0},
	.ap_subnet_mask = {0, 0, 0, 0},
	.ap_gateway = {0, 0, 0, 0},
	.ap_mac_address = {0, 0, 0, 0, 0, 0},
	.ap_rx_count = 0,
	.ap_tx_count = 0,
	.ap_connected_count = 0,
};

bool ICACHE_FLASH_ATTR com_handle_message(const uint8_t *data, const uint8_t length, const int8_t cid) {
	const MessageHeader *header = (const MessageHeader*)data;

	if(!brickd_check_auth(header, cid)) {
		return true;
	}

	if(header->uid == 1) {
		switch(header->fid) {
			case BRICKD_FID_GET_AUTHENTICATION_NONCE: brickd_get_authentication_nonce(cid, (GetAuthenticationNonce*)data); return true;
			case BRICKD_FID_AUTHENTICATE: brickd_authenticate(cid, (Authenticate*)data); return true;
		}
	}

	switch(header->fid) {
		case FID_SET_WIFI2_AUTHENTICATION_SECRET: set_wifi2_authentication_secret(cid, (SetWifi2AuthenticationSecret*)data); return true;
		case FID_GET_WIFI2_AUTHENTICATION_SECRET: get_wifi2_authentication_secret(cid, (GetWifi2AuthenticationSecret*)data); return true;
		case FID_SET_WIFI2_CONFIGURATION:         set_wifi2_configuration(cid, (SetWifi2Configuration*)data);                return true;
		case FID_GET_WIFI2_CONFIGURATION:         get_wifi2_configuration(cid, (GetWifi2Configuration*)data);                return true;
		case FID_GET_WIFI2_STATUS:                get_wifi2_status(cid, (GetWifi2Status*)data);                              return true;
		case FID_SET_WIFI2_CLIENT_CONFIGURATION:  set_wifi2_client_configuration(cid, (SetWifi2ClientConfiguration*)data);   return true;
		case FID_GET_WIFI2_CLIENT_CONFIGURATION:  get_wifi2_client_configuration(cid, (GetWifi2ClientConfiguration*)data);   return true;
		case FID_SET_WIFI2_CLIENT_HOSTNAME:       set_wifi2_client_hostname(cid, (SetWifi2ClientHostname*)data);             return true;
		case FID_GET_WIFI2_CLIENT_HOSTNAME:       get_wifi2_client_hostname(cid, (GetWifi2ClientHostname*)data);             return true;
		case FID_SET_WIFI2_CLIENT_PASSWORD:       set_wifi2_client_password(cid, (SetWifi2ClientPassword*)data);             return true;
		case FID_GET_WIFI2_CLIENT_PASSWORD:       get_wifi2_client_password(cid, (GetWifi2ClientPassword*)data);             return true;
		case FID_SET_WIFI2_AP_CONFIGURATION:      set_wifi2_ap_configuration(cid, (SetWifi2APConfiguration*)data);           return true;
		case FID_GET_WIFI2_AP_CONFIGURATION:      get_wifi2_ap_configuration(cid, (GetWifi2APConfiguration*)data);           return true;
		case FID_SET_WIFI2_AP_PASSWORD:           set_wifi2_ap_password(cid, (SetWifi2APPassword*)data);                     return true;
		case FID_GET_WIFI2_AP_PASSWORD:           get_wifi2_ap_password(cid, (GetWifi2APPassword*)data);                     return true;
		case FID_SAVE_WIFI2_CONFIGURATION:        save_wifi2_configuration(cid, (SaveWifi2Configuration*)data);              return true;
		case FID_GET_WIFI2_FIRMWARE_VERSION:      get_wifi2_firmware_version(cid, (GetWifi2FirmwareVersion*)data);           return true;
		case FID_ENABLE_WIFI2_STATUS_LED:         enable_wifi2_status_led(cid, (EnableWifi2StatusLED*)data);                 return true;
		case FID_DISABLE_WIFI2_STATUS_LED:        disable_wifi2_status_led(cid, (DisableWifi2StatusLED*)data);               return true;
		case FID_IS_WIFI2_STATUS_LED_ENABLED:     is_wifi2_status_led_enabled(cid, (IsWifi2StatusLEDEnabled*)data);          return true;
	}

	return false;
}

void ICACHE_FLASH_ATTR com_send(const void *data, const uint8_t length, const int8_t cid) {
	if(cid == -2) { // UART
		tfp_handle_packet(data, length);
	} else { // WIFI
		const bool ok = tfp_send_w_cid(data, length, cid);
		// TOOD: What do we do here if the sending didn't work?
		//       Do we need to have a send buffer for this?
	}
}

void ICACHE_FLASH_ATTR com_return_error(const void *data, const uint8_t ret_length, const uint8_t error_code, const int8_t cid) {
	MessageHeader *message = (MessageHeader*)data;

	if(!message->return_expected) {
		return;
	}

	uint8_t ret_data[ret_length];
	MessageHeader *ret_message = (MessageHeader*)&ret_data;

	os_memset(ret_data, 0, ret_length);
	*ret_message = *message;
	ret_message->length = ret_length;
	ret_message->error = error_code;

	com_send(ret_data, ret_length, cid);
}

void ICACHE_FLASH_ATTR com_return_setter(const int8_t cid, const void *data) {
	if(((MessageHeader*)data)->return_expected) {
		MessageHeader ret = *((MessageHeader*)data);
		ret.length = sizeof(MessageHeader);
		com_send(&ret, sizeof(MessageHeader), cid);
	}
}

void ICACHE_FLASH_ATTR set_wifi2_authentication_secret(const int8_t cid, const SetWifi2AuthenticationSecret *data) {
	os_memcpy(configuration_current.general_authentication_secret, data->secret, CONFIGURATION_SECRET_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_authentication_secret(const int8_t cid, const GetWifi2AuthenticationSecret *data) {
	GetWifi2AuthenticationSecretReturn gw2asr;

	gw2asr.header        = data->header;
	gw2asr.header.length = sizeof(GetWifi2AuthenticationSecretReturn);
	os_memcpy(gw2asr.secret, configuration_current.general_authentication_secret, CONFIGURATION_SECRET_MAX_LENGTH);

	com_send(&gw2asr, sizeof(GetWifi2AuthenticationSecretReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_configuration(const int8_t cid, const SetWifi2Configuration *data) {
	configuration_current.general_port           = data->port;
	configuration_current.general_websocket_port = data->websocket_port;
	configuration_current.general_website_port   = data->website_port;
	configuration_current.general_phy_mode       = data->phy_mode + 1;
	configuration_current.general_sleep_mode     = data->sleep_mode;
	configuration_current.general_website        = data->website;

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_configuration(const int8_t cid, const GetWifi2Configuration *data) {
	GetWifi2ConfigurationReturn gw2cr;

	gw2cr.header         = data->header;
	gw2cr.header.length  = sizeof(GetWifi2ConfigurationReturn);
	gw2cr.port           = configuration_current.general_port;
	gw2cr.websocket_port = configuration_current.general_websocket_port;
	gw2cr.website_port   = configuration_current.general_website_port;
	gw2cr.phy_mode       = configuration_current.general_phy_mode;
	gw2cr.sleep_mode     = configuration_current.general_sleep_mode;
	gw2cr.website        = configuration_current.general_website;

	com_send(&gw2cr, sizeof(GetWifi2ConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR get_wifi2_status(const int8_t cid, const GetWifi2Status *data) {
	gw2sr.header = data->header;
	gw2sr.header.length = sizeof(GetWifi2StatusReturn);

	struct ip_info info;
	wifi_get_ip_info(STATION_IF, &info);
	gw2sr.client_ip[0] = ip4_addr1(&info.ip);
	gw2sr.client_ip[1] = ip4_addr2(&info.ip);
	gw2sr.client_ip[2] = ip4_addr3(&info.ip);
	gw2sr.client_ip[3] = ip4_addr4(&info.ip);
	gw2sr.client_subnet_mask[0] = ip4_addr1(&info.netmask);
	gw2sr.client_subnet_mask[1] = ip4_addr2(&info.netmask);
	gw2sr.client_subnet_mask[2] = ip4_addr3(&info.netmask);
	gw2sr.client_subnet_mask[3] = ip4_addr4(&info.netmask);
	gw2sr.client_gateway[0] = ip4_addr1(&info.gw);
	gw2sr.client_gateway[1] = ip4_addr2(&info.gw);
	gw2sr.client_gateway[2] = ip4_addr3(&info.gw);
	gw2sr.client_gateway[3] = ip4_addr4(&info.gw);

	wifi_get_ip_info(SOFTAP_IF, &info);
	gw2sr.ap_ip[0] = ip4_addr1(&info.ip);
	gw2sr.ap_ip[1] = ip4_addr2(&info.ip);
	gw2sr.ap_ip[2] = ip4_addr3(&info.ip);
	gw2sr.ap_ip[3] = ip4_addr4(&info.ip);
	gw2sr.ap_subnet_mask[0] = ip4_addr1(&info.netmask);
	gw2sr.ap_subnet_mask[1] = ip4_addr2(&info.netmask);
	gw2sr.ap_subnet_mask[2] = ip4_addr3(&info.netmask);
	gw2sr.ap_subnet_mask[3] = ip4_addr4(&info.netmask);
	gw2sr.ap_gateway[0] = ip4_addr1(&info.gw);
	gw2sr.ap_gateway[1] = ip4_addr2(&info.gw);
	gw2sr.ap_gateway[2] = ip4_addr3(&info.gw);
	gw2sr.ap_gateway[3] = ip4_addr4(&info.gw);

	wifi_get_macaddr(STATION_IF, gw2sr.client_mac_address);
	wifi_get_macaddr(SOFTAP_IF, gw2sr.ap_mac_address);

	gw2sr.client_enabled = configuration_current.client_enable;
	gw2sr.ap_enabled = configuration_current.ap_enable;

	gw2sr.client_rssi = wifi_station_get_rssi();

	gw2sr.client_status = wifi_station_get_connect_status();

	gw2sr.ap_connected_count = wifi_softap_get_station_num();

	com_send(&gw2sr, sizeof(GetWifi2StatusReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_client_configuration(const int8_t cid, const SetWifi2ClientConfiguration *data) {
	configuration_current.client_enable = data->enable;
	os_memcpy(configuration_current.client_ssid, data->ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(configuration_current.client_ip, data->ip, 4);
	os_memcpy(configuration_current.client_subnet_mask, data->subnet_mask, 4);
	os_memcpy(configuration_current.client_gateway, data->gateway, 4);
	os_memcpy(configuration_current.client_mac_address, data->mac_address, 6);
	os_memcpy(configuration_current.client_bssid, data->bssid, 6);

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_client_configuration(const int8_t cid, const GetWifi2ClientConfiguration *data) {
	GetWifi2ClientConfigurationReturn gw2ccr;

	gw2ccr.header        = data->header;
	gw2ccr.header.length = sizeof(GetWifi2ClientConfigurationReturn);
	gw2ccr.enable        = configuration_current.client_enable;
	os_memcpy(gw2ccr.ssid, configuration_current.client_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(gw2ccr.ip, configuration_current.client_ip, 4);
	os_memcpy(gw2ccr.subnet_mask, configuration_current.client_subnet_mask, 4);
	os_memcpy(gw2ccr.gateway, configuration_current.client_gateway, 4);
	os_memcpy(gw2ccr.mac_address, configuration_current.client_mac_address, 6);
	os_memcpy(gw2ccr.bssid, configuration_current.client_bssid, 6);

	com_send(&gw2ccr, sizeof(GetWifi2ClientConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_client_hostname(const int8_t cid, const SetWifi2ClientHostname *data) {
	os_memcpy(configuration_current.client_hostname, data->hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_client_hostname(const int8_t cid, const GetWifi2ClientHostname *data) {
	GetWifi2ClientHostnameReturn gw2chr;

	gw2chr.header        = data->header;
	gw2chr.header.length = sizeof(GetWifi2ClientHostnameReturn);
	os_memcpy(gw2chr.hostname, configuration_current.client_hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);

	com_send(&gw2chr, sizeof(GetWifi2ClientHostnameReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_client_password(const int8_t cid, const SetWifi2ClientPassword *data) {
	os_memcpy(configuration_current.client_password, data->password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_client_password(const int8_t cid, const GetWifi2ClientPassword *data) {
	GetWifi2ClientPasswordReturn gw2cpr;

	gw2cpr.header        = data->header;
	gw2cpr.header.length = sizeof(GetWifi2ClientPasswordReturn);
	os_memcpy(gw2cpr.password, configuration_current.client_password, CONFIGURATION_PASSWORD_MAX_LENGTH);

	com_send(&gw2cpr, sizeof(GetWifi2ClientPasswordReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_ap_configuration(const int8_t cid, const SetWifi2APConfiguration *data) {
	configuration_current.ap_enable     = data->enable;
	os_memcpy(configuration_current.ap_ssid, data->ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(configuration_current.ap_ip, data->ip, 4);
	os_memcpy(configuration_current.ap_subnet_mask, data->subnet_mask, 4);
	os_memcpy(configuration_current.ap_gateway, data->gateway, 4);

	if(data->encryption > 0){
			configuration_current.ap_encryption = data->encryption + 1;
	}
	else{
			configuration_current.ap_encryption = data->encryption;
	}

	configuration_current.ap_hidden     = data->hidden;
	configuration_current.ap_channel    = data->channel;
	os_memcpy(configuration_current.ap_mac_address, data->mac_address, 6);

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_ap_configuration(const int8_t cid, const GetWifi2APConfiguration *data) {
	GetWifi2APConfigurationReturn gw2apcr;

	gw2apcr.header        = data->header;
	gw2apcr.header.length = sizeof(GetWifi2APConfigurationReturn);
	gw2apcr.enable        = configuration_current.ap_enable;
	os_memcpy(gw2apcr.ssid, configuration_current.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(gw2apcr.ip, configuration_current.ap_ip, 4);
	os_memcpy(gw2apcr.subnet_mask, configuration_current.ap_subnet_mask, 4);
	os_memcpy(gw2apcr.gateway, configuration_current.ap_gateway, 4);
	gw2apcr.encryption    = configuration_current.ap_encryption;
	gw2apcr.hidden        = configuration_current.ap_hidden;
	gw2apcr.channel       = configuration_current.ap_channel;
	os_memcpy(gw2apcr.mac_address, configuration_current.ap_mac_address, 6);

	com_send(&gw2apcr, sizeof(GetWifi2APConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_ap_password(const int8_t cid, const SetWifi2APPassword *data) {
	os_memcpy(configuration_current.ap_password, data->password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_ap_password(const int8_t cid, const GetWifi2APPassword *data) {
	GetWifi2APPasswordReturn gw2appr;

	gw2appr.header        = data->header;
	gw2appr.header.length = sizeof(GetWifi2APPasswordReturn);
	os_memcpy(gw2appr.password, configuration_current.ap_password, CONFIGURATION_PASSWORD_MAX_LENGTH);

	com_send(&gw2appr, sizeof(GetWifi2APPasswordReturn), cid);
}

void ICACHE_FLASH_ATTR save_wifi2_configuration(const int8_t cid, const SaveWifi2Configuration *data) {
	SaveWifi2ConfigurationReturn sw2cr;

	sw2cr.header        = data->header;
	sw2cr.header.length = sizeof(SaveWifi2ConfigurationReturn);
	sw2cr.result        = configuration_save_to_eeprom();

	com_send(&sw2cr, sizeof(SaveWifi2ConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR get_wifi2_firmware_version(const int8_t cid, const GetWifi2FirmwareVersion *data) {
	GetWifi2FirmwareVersionReturn gw2fvr;

	gw2fvr.header        = data->header;
	gw2fvr.header.length = sizeof(GetWifi2FirmwareVersionReturn);
	gw2fvr.version_fw[0] = FIRMWARE_VERSION_MAJOR;
	gw2fvr.version_fw[1] = FIRMWARE_VERSION_MINOR;
	gw2fvr.version_fw[2] = FIRMWARE_VERSION_REVISION;

	com_send(&gw2fvr, sizeof(GetWifi2FirmwareVersionReturn), cid);
}

void ICACHE_FLASH_ATTR enable_wifi2_status_led(const int8_t cid, const EnableWifi2StatusLED *data) {
	if(!wifi2_status_led_enabled) {
		wifi_status_led_install(12, PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
		wifi2_status_led_enabled = true;
	}
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR disable_wifi2_status_led(const int8_t cid, const DisableWifi2StatusLED *data) {
	if(wifi2_status_led_enabled) {
		wifi_status_led_uninstall();
		gpio_output_set(BIT12, 0, BIT12, 0);
		wifi2_status_led_enabled = false;
	}
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR is_wifi2_status_led_enabled(const int8_t cid, const IsWifi2StatusLEDEnabled *data) {
	IsWifi2StatusLEDEnabledReturn iw2sleder;

	iw2sleder.header        = data->header;
	iw2sleder.header.length = sizeof(IsWifi2StatusLEDEnabledReturn);
	iw2sleder.enabled       = wifi2_status_led_enabled;

	com_send(&iw2sleder, sizeof(IsWifi2StatusLEDEnabledReturn), cid);
}
