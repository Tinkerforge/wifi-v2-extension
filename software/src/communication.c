/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 * Copyright (C) 2020 Matthias Bolte <matthias@tinkerforge.com>
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
#include "ringbuffer.h"
#include "mesh.h"
#include <string.h>
#include <esp8266.h>

bool wifi2_status_led_enabled = true;
extern Configuration configuration_current;
extern Configuration configuration_current_to_save;

Ringbuffer com_out_rb;
uint8_t com_out_rb_buffer[COM_OUT_RINGBUFFER_LENGTH];

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

GetWifi2MeshCommonStatusReturn gw2mcsr = {
	.status = 0,
	.is_root_node = false,
	.is_root_candidate = false,
	.connected_nodes = 0,
	.rx_count = 0,
	.tx_count = 0,
};

GetWifi2MeshClientStatusReturn gw2mssr = {
	.hostname = "\0",
	.ip = {0, 0, 0, 0},
	.sub = {0, 0, 0, 0},
	.gw = {0, 0, 0, 0},
	.mac = {0, 0, 0, 0, 0, 0},
};

GetWifi2MeshAPStatusReturn gw2masr = {
	.ssid = "\0",
	.ip = {0, 0, 0, 0},
	.sub = {0, 0, 0, 0},
	.gw = {0, 0, 0, 0},
	.mac = {0, 0, 0, 0, 0, 0},
};

void ICACHE_FLASH_ATTR com_init(void) {
	ringbuffer_init(&com_out_rb, com_out_rb_buffer, COM_OUT_RINGBUFFER_LENGTH);
}

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

		com_return_error(data, sizeof(MessageHeader), MESSAGE_ERROR_CODE_NOT_SUPPORTED, cid);
		return true;
	}

	// TODO: Check for UID of master of stack Master Brick
	//       Where do we get the UID?
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
		case FID_SET_WIFI2_MESH_CONFIGURATION:    set_wifi2_mesh_configuration(cid, (SetWifi2MeshConfiguration*)data);       return true;
		case FID_GET_WIFI2_MESH_CONFIGURATION:    get_wifi2_mesh_configuration(cid, (GetWifi2MeshConfiguration*)data);       return true;
		case FID_SET_WIFI2_MESH_ROUTER_SSID:      set_wifi2_mesh_router_ssid(cid, (SetWifi2MeshRouterSSID*)data);						 return true;
		case FID_GET_WIFI2_MESH_ROUTER_SSID:      get_wifi2_mesh_router_ssid(cid, (GetWifi2MeshRouterSSID*)data);						 return true;
		case FID_SET_WIFI2_MESH_ROUTER_PASSWORD:  set_wifi2_mesh_router_password(cid, (SetWifi2MeshRouterPassword*)data);		 return true;
		case FID_GET_WIFI2_MESH_ROUTER_PASSWORD:  get_wifi2_mesh_router_password(cid, (GetWifi2MeshRouterPassword*)data);		 return true;
		case FID_GET_WIFI2_MESH_COMMON_STATUS:    get_wifi2_mesh_common_status(cid, (GetWifi2MeshCommonStatus*)data);    		 return true;
		case FID_GET_WIFI2_MESH_CLIENT_STATUS:    get_wifi2_mesh_client_status(cid, (GetWifi2MeshClientStatus*)data);  		   return true;
		case FID_GET_WIFI2_MESH_AP_STATUS:        get_wifi2_mesh_ap_status(cid, (GetWifi2MeshAPStatus*)data);            		 return true;
	}

	return false;
}

void ICACHE_FLASH_ATTR com_send(const void *data, const uint8_t length, const int8_t cid) {
	if(cid == -2) { // UART
		tfp_handle_packet(data, length);
	} else { // WIFI
		// If ringbuffer not empty we add data to ringbuffer (we want to keep order)
		// Else if we can't immediately send to master brick we also add to ringbuffer
		if(!ringbuffer_is_empty(&com_out_rb) || tfp_send_w_cid(data, length, cid) == 0) {

			if(ringbuffer_get_free(&com_out_rb) > (length + 2 + 1)) {
				ringbuffer_add(&com_out_rb, cid);
				for(uint8_t i = 0; i < length; i++) {
					if(!ringbuffer_add(&com_out_rb, ((uint8_t*)data)[i])) {
						// Should be unreachable
						loge("Ringbuffer (com out) full!\n");
					}
				}
			} else {
				logw("Message does not fit in Buffer (com out): %d < %d\n", ringbuffer_get_free(&com_out_rb), length + 2);
			}
		}
	}
}

void ICACHE_FLASH_ATTR com_poll(void) {
	if(ringbuffer_is_empty(&com_out_rb)) {
		return;
	}

	// cid is in data[0], TFP packet has off by one, use +1 for access
	uint8_t data[TFP_RECV_BUFFER_SIZE + 1];
	uint8_t length = ringbuffer_peak(&com_out_rb, data, TFP_RECV_BUFFER_SIZE + 1);

	if(length > TFP_RECV_BUFFER_SIZE + 1) {
		ringbuffer_init(&com_out_rb, com_out_rb.buffer, com_out_rb.buffer_length);
		logd("com_poll: length > %d: %d\n", TFP_RECV_BUFFER_SIZE, length);
		return;
	}

	if(length < TFP_MIN_LENGTH + 1) {
		ringbuffer_init(&com_out_rb, com_out_rb.buffer, com_out_rb.buffer_length);
		logd("com_poll: length < TFP_MIN_LENGTH\n");
		return;
	}

	if(tfp_send_w_cid(data + 1, data[TFP_RECV_INDEX_LENGTH + 1], data[0]) != 0) {
		ringbuffer_remove(&com_out_rb, data[TFP_RECV_INDEX_LENGTH + 1] + 1);
	} else {
		//logd("com_poll send error\n");
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
	os_memcpy(configuration_current_to_save.general_authentication_secret, data->secret, CONFIGURATION_SECRET_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_authentication_secret(const int8_t cid, const GetWifi2AuthenticationSecret *data) {
	GetWifi2AuthenticationSecretReturn gw2asr;

	gw2asr.header        = data->header;
	gw2asr.header.length = sizeof(GetWifi2AuthenticationSecretReturn);
	os_memcpy(gw2asr.secret, configuration_current_to_save.general_authentication_secret, CONFIGURATION_SECRET_MAX_LENGTH);

	com_send(&gw2asr, sizeof(GetWifi2AuthenticationSecretReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_configuration(const int8_t cid, const SetWifi2Configuration *data) {
	char str_fw_version[4];

	os_bzero(str_fw_version, sizeof(str_fw_version));
	os_sprintf(str_fw_version, "%d%d%d", FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR,
		FIRMWARE_VERSION_REVISION);

	configuration_current_to_save.general_port           = data->port;
	configuration_current_to_save.general_websocket_port = data->websocket_port;
	configuration_current_to_save.general_website_port   = data->website_port;
	configuration_current_to_save.general_phy_mode       = data->phy_mode;
	configuration_current_to_save.general_sleep_mode     = data->sleep_mode;
	configuration_current_to_save.general_website        = data->website;

	if(strtoul(str_fw_version, NULL, 10) >= 204) {
		if(data->website != 1) {
			configuration_current_to_save.general_website_port = 1;
		}
	}

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_configuration(const int8_t cid, const GetWifi2Configuration *data) {
	GetWifi2ConfigurationReturn gw2cr;

	gw2cr.header         = data->header;
	gw2cr.header.length  = sizeof(GetWifi2ConfigurationReturn);
	gw2cr.port           = configuration_current_to_save.general_port;
	gw2cr.websocket_port = configuration_current_to_save.general_websocket_port;
	gw2cr.website_port   = configuration_current_to_save.general_website_port;
	gw2cr.phy_mode       = configuration_current_to_save.general_phy_mode;
	gw2cr.sleep_mode     = configuration_current_to_save.general_sleep_mode;
	gw2cr.website        = configuration_current_to_save.general_website;

	com_send(&gw2cr, sizeof(GetWifi2ConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_mesh_configuration(const int8_t cid,
                                                    const SetWifi2MeshConfiguration *data) {
	configuration_current_to_save.mesh_enable = data->mesh_enable;

	os_memcpy(configuration_current_to_save.mesh_root_ip, data->mesh_root_ip,
	          sizeof(data->mesh_root_ip));

	os_memcpy(configuration_current_to_save.mesh_root_subnet_mask, data->mesh_root_subnet_mask,
	          sizeof(data->mesh_root_subnet_mask));

	os_memcpy(configuration_current_to_save.mesh_root_gateway, data->mesh_root_gateway,
	          sizeof(data->mesh_root_gateway));

	os_memcpy(configuration_current_to_save.mesh_router_bssid, data->mesh_router_bssid,
	          sizeof(data->mesh_router_bssid));

	os_memcpy(configuration_current_to_save.mesh_group_id, data->mesh_group_id,
	          sizeof(data->mesh_group_id));

	os_bzero(configuration_current_to_save.mesh_group_ssid_prefix,
	         sizeof(configuration_current_to_save.mesh_group_ssid_prefix));

	os_memcpy(configuration_current_to_save.mesh_group_ssid_prefix, data->mesh_group_ssid_prefix,
	          sizeof(data->mesh_group_ssid_prefix));

	os_memcpy(configuration_current_to_save.mesh_gateway_ip, data->mesh_gateway_ip,
	          sizeof(data->mesh_gateway_ip));

	configuration_current_to_save.mesh_gateway_port = data->mesh_gateway_port;

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_mesh_configuration(const int8_t cid,
                                                    const GetWifi2MeshConfiguration *data) {
	GetWifi2MeshConfigurationReturn gw2mcr;

	os_bzero(&gw2mcr, sizeof(GetWifi2MeshConfigurationReturn));

	gw2mcr.header         = data->header;
	gw2mcr.header.length  = sizeof(GetWifi2MeshConfigurationReturn);

	gw2mcr.mesh_enable    = configuration_current_to_save.mesh_enable;

	os_memcpy(gw2mcr.mesh_root_ip, configuration_current_to_save.mesh_root_ip,
	          sizeof(configuration_current_to_save.mesh_root_ip));

	os_memcpy(gw2mcr.mesh_root_subnet_mask, configuration_current_to_save.mesh_root_subnet_mask,
	          sizeof(configuration_current_to_save.mesh_root_subnet_mask));

	os_memcpy(gw2mcr.mesh_root_gateway, configuration_current_to_save.mesh_root_gateway,
	          sizeof(configuration_current_to_save.mesh_root_gateway));

	os_memcpy(gw2mcr.mesh_router_bssid, configuration_current_to_save.mesh_router_bssid,
	          sizeof(configuration_current_to_save.mesh_router_bssid));

	os_memcpy(gw2mcr.mesh_group_id, configuration_current_to_save.mesh_group_id,
	          sizeof(configuration_current_to_save.mesh_group_id));

	os_memcpy(gw2mcr.mesh_group_ssid_prefix, configuration_current_to_save.mesh_group_ssid_prefix,
	          sizeof(configuration_current_to_save.mesh_group_ssid_prefix));

	os_memcpy(gw2mcr.mesh_gateway_ip, configuration_current_to_save.mesh_gateway_ip,
	          sizeof(configuration_current_to_save.mesh_gateway_ip));

	gw2mcr.mesh_gateway_port = configuration_current_to_save.mesh_gateway_port;

	com_send(&gw2mcr, sizeof(GetWifi2MeshConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_mesh_router_ssid(const int8_t cid,
                                                  const SetWifi2MeshRouterSSID *data) {
	os_bzero(configuration_current_to_save.mesh_router_ssid, sizeof(configuration_current_to_save.mesh_router_ssid));

	os_memcpy(configuration_current_to_save.mesh_router_ssid, data->mesh_router_ssid,
	          sizeof(data->mesh_router_ssid));

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_mesh_router_ssid(const int8_t cid, const GetWifi2MeshRouterSSID *data) {
	GetWifi2MeshRouterSSIDReturn gw2mrsr;

	os_bzero(&gw2mrsr, sizeof(GetWifi2MeshRouterSSID));

	gw2mrsr.header         = data->header;
	gw2mrsr.header.length  = sizeof(GetWifi2MeshRouterSSIDReturn);

	os_memcpy(gw2mrsr.mesh_router_ssid, configuration_current_to_save.mesh_router_ssid,
	          sizeof(gw2mrsr.mesh_router_ssid));

	com_send(&gw2mrsr, sizeof(GetWifi2MeshRouterSSIDReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_mesh_router_password(const int8_t cid,
                                                      const SetWifi2MeshRouterPassword *data) {
	os_bzero(configuration_current_to_save.mesh_router_password,
	         sizeof(configuration_current_to_save.mesh_router_password));

	os_memcpy(configuration_current_to_save.mesh_router_password, data->mesh_router_password,
	          sizeof(data->mesh_router_password));

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_mesh_router_password(const int8_t cid,
                                                      const GetWifi2MeshRouterPassword *data) {
	GetWifi2MeshRouterPasswordReturn gw2mrpr;

	os_bzero(&gw2mrpr, sizeof(GetWifi2MeshRouterPasswordReturn));

	gw2mrpr.header         = data->header;
	gw2mrpr.header.length  = sizeof(GetWifi2MeshRouterPasswordReturn);

	os_memcpy(gw2mrpr.mesh_router_password, configuration_current_to_save.mesh_router_password,
		sizeof(configuration_current_to_save.mesh_router_password));

	com_send(&gw2mrpr, sizeof(GetWifi2MeshRouterPasswordReturn), cid);
}

void ICACHE_FLASH_ATTR get_wifi2_mesh_common_status(const int8_t cid,
                                                    const GetWifi2MeshCommonStatus *data) {
	os_bzero(&gw2mcsr.header, sizeof(gw2mcsr.header));

	gw2mcsr.header = data->header;
	gw2mcsr.header.length = sizeof(GetWifi2MeshCommonStatusReturn);

	gw2mcsr.status = espconn_mesh_get_status();
	gw2mcsr.is_root_node = espconn_mesh_is_root();
	gw2mcsr.is_root_candidate = espconn_mesh_is_root_candidate();
	gw2mcsr.connected_nodes = espconn_mesh_get_sub_dev_count();

	com_send(&gw2mcsr, sizeof(GetWifi2MeshCommonStatusReturn), cid);
}

void ICACHE_FLASH_ATTR get_wifi2_mesh_client_status(const int8_t cid,
                                                    const GetWifi2MeshClientStatus *data) {
	uint8_t mac[6];
	char *hostname_ptr;
	struct ip_info info_ipv4;
	struct station_config *config_st;

	os_bzero(&gw2mssr.header, sizeof(gw2mssr.header));

	gw2mssr.header = data->header;
	gw2mssr.header.length = sizeof(GetWifi2MeshClientStatusReturn);

	if((wifi_get_ip_info(STATION_IF, &info_ipv4)) && (wifi_get_macaddr(STATION_IF, mac))) {
		hostname_ptr = wifi_station_get_hostname();

		os_bzero(gw2mssr.hostname, sizeof(gw2mssr.hostname));
		os_memcpy(gw2mssr.hostname, hostname_ptr, sizeof(gw2mssr.hostname));

		os_bzero(gw2mssr.ip, sizeof(gw2mssr.ip));
		os_memcpy(gw2mssr.ip, (uint8_t *)&info_ipv4.ip.addr, sizeof(info_ipv4.ip.addr));

		os_bzero(gw2mssr.sub, sizeof(gw2mssr.sub));
		os_memcpy(gw2mssr.sub, (uint8_t *)&info_ipv4.netmask.addr, sizeof(info_ipv4.netmask.addr));

		os_bzero(gw2mssr.gw, sizeof(gw2mssr.gw));
		os_memcpy(gw2mssr.gw, (uint8_t *)&info_ipv4.gw.addr, sizeof(info_ipv4.gw.addr));

		os_bzero(gw2mssr.mac, sizeof(gw2mssr.mac));
		os_memcpy(gw2mssr.mac, mac, sizeof(mac));
	}

	com_send(&gw2mssr, sizeof(GetWifi2MeshClientStatusReturn), cid);
}

void ICACHE_FLASH_ATTR get_wifi2_mesh_ap_status(const int8_t cid,
                                                const GetWifi2MeshAPStatus *data) {
	uint8_t mac[6];
	struct ip_info info_ipv4;
	struct softap_config config_ap;

	os_bzero(&gw2masr.header, sizeof(gw2masr.header));

	gw2masr.header = data->header;
	gw2masr.header.length = sizeof(GetWifi2MeshAPStatusReturn);

	if((wifi_softap_get_config(&config_ap)) && (wifi_get_ip_info(SOFTAP_IF, &info_ipv4)) \
	&& (wifi_get_macaddr(SOFTAP_IF, mac))) {
		os_bzero(gw2masr.ssid, sizeof(gw2masr.ssid));
		os_memcpy(gw2masr.ssid, config_ap.ssid, sizeof(config_ap.ssid));

		os_bzero(gw2masr.ip, sizeof(gw2masr.ip));
		os_memcpy(gw2masr.ip, (uint8_t *)&info_ipv4.ip.addr, sizeof(info_ipv4.ip.addr));

		os_bzero(gw2masr.sub, sizeof(gw2masr.sub));
		os_memcpy(gw2masr.sub, (uint8_t *)&info_ipv4.netmask.addr, sizeof(info_ipv4.ip.addr));

		os_bzero(gw2masr.gw, sizeof(gw2masr.gw));
		os_memcpy(gw2masr.gw, (uint8_t *)&info_ipv4.gw.addr, sizeof(info_ipv4.ip.addr));

		os_bzero(gw2masr.mac, sizeof(gw2masr.mac));
		os_memcpy(gw2masr.mac, mac, sizeof(mac));
	}

	com_send(&gw2masr, sizeof(GetWifi2MeshAPStatusReturn), cid);
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

	gw2sr.client_enabled = configuration_current_to_save.client_enable;
	gw2sr.ap_enabled = configuration_current_to_save.ap_enable;

	gw2sr.client_rssi = wifi_station_get_rssi();

	gw2sr.client_status = wifi_station_get_connect_status();

	gw2sr.ap_connected_count = wifi_softap_get_station_num();

	com_send(&gw2sr, sizeof(GetWifi2StatusReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_client_configuration(const int8_t cid, const SetWifi2ClientConfiguration *data) {
	configuration_current_to_save.client_enable = data->enable;
	os_memcpy(configuration_current_to_save.client_ssid, data->ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(configuration_current_to_save.client_ip, data->ip, 4);
	os_memcpy(configuration_current_to_save.client_subnet_mask, data->subnet_mask, 4);
	os_memcpy(configuration_current_to_save.client_gateway, data->gateway, 4);
	os_memcpy(configuration_current_to_save.client_mac_address, data->mac_address, 6);
	os_memcpy(configuration_current_to_save.client_bssid, data->bssid, 6);

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_client_configuration(const int8_t cid, const GetWifi2ClientConfiguration *data) {
	GetWifi2ClientConfigurationReturn gw2ccr;

	gw2ccr.header        = data->header;
	gw2ccr.header.length = sizeof(GetWifi2ClientConfigurationReturn);
	gw2ccr.enable        = configuration_current_to_save.client_enable;
	os_memcpy(gw2ccr.ssid, configuration_current_to_save.client_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(gw2ccr.ip, configuration_current_to_save.client_ip, 4);
	os_memcpy(gw2ccr.subnet_mask, configuration_current_to_save.client_subnet_mask, 4);
	os_memcpy(gw2ccr.gateway, configuration_current_to_save.client_gateway, 4);
	os_memcpy(gw2ccr.mac_address, configuration_current_to_save.client_mac_address, 6);
	os_memcpy(gw2ccr.bssid, configuration_current_to_save.client_bssid, 6);

	com_send(&gw2ccr, sizeof(GetWifi2ClientConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_client_hostname(const int8_t cid, const SetWifi2ClientHostname *data) {
	os_memcpy(configuration_current_to_save.client_hostname, data->hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_client_hostname(const int8_t cid, const GetWifi2ClientHostname *data) {
	GetWifi2ClientHostnameReturn gw2chr;

	gw2chr.header        = data->header;
	gw2chr.header.length = sizeof(GetWifi2ClientHostnameReturn);
	os_memcpy(gw2chr.hostname, configuration_current_to_save.client_hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);

	com_send(&gw2chr, sizeof(GetWifi2ClientHostnameReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_client_password(const int8_t cid, const SetWifi2ClientPassword *data) {
	os_memcpy(configuration_current_to_save.client_password, data->password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_client_password(const int8_t cid, const GetWifi2ClientPassword *data) {
	GetWifi2ClientPasswordReturn gw2cpr;

	gw2cpr.header        = data->header;
	gw2cpr.header.length = sizeof(GetWifi2ClientPasswordReturn);
	os_memset(gw2cpr.password, '\0', CONFIGURATION_PASSWORD_MAX_LENGTH);

	com_send(&gw2cpr, sizeof(GetWifi2ClientPasswordReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_ap_configuration(const int8_t cid, const SetWifi2APConfiguration *data) {
	configuration_current_to_save.ap_enable     = data->enable;
	os_memcpy(configuration_current_to_save.ap_ssid, data->ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(configuration_current_to_save.ap_ip, data->ip, 4);
	os_memcpy(configuration_current_to_save.ap_subnet_mask, data->subnet_mask, 4);
	os_memcpy(configuration_current_to_save.ap_gateway, data->gateway, 4);
	configuration_current_to_save.ap_encryption = data->encryption;
	configuration_current_to_save.ap_hidden     = data->hidden;
	configuration_current_to_save.ap_channel    = data->channel;
	os_memcpy(configuration_current_to_save.ap_mac_address, data->mac_address, 6);

	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_ap_configuration(const int8_t cid, const GetWifi2APConfiguration *data) {
	GetWifi2APConfigurationReturn gw2apcr;

	gw2apcr.header        = data->header;
	gw2apcr.header.length = sizeof(GetWifi2APConfigurationReturn);
	gw2apcr.enable        = configuration_current_to_save.ap_enable;
	os_memcpy(gw2apcr.ssid, configuration_current_to_save.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	os_memcpy(gw2apcr.ip, configuration_current_to_save.ap_ip, 4);
	os_memcpy(gw2apcr.subnet_mask, configuration_current_to_save.ap_subnet_mask, 4);
	os_memcpy(gw2apcr.gateway, configuration_current_to_save.ap_gateway, 4);
	gw2apcr.encryption    = configuration_current_to_save.ap_encryption;
	gw2apcr.hidden        = configuration_current_to_save.ap_hidden;
	gw2apcr.channel       = configuration_current_to_save.ap_channel;
	os_memcpy(gw2apcr.mac_address, configuration_current_to_save.ap_mac_address, 6);

	com_send(&gw2apcr, sizeof(GetWifi2APConfigurationReturn), cid);
}

void ICACHE_FLASH_ATTR set_wifi2_ap_password(const int8_t cid, const SetWifi2APPassword *data) {
	if (strnlen(data->password, CONFIGURATION_PASSWORD_MAX_LENGTH) < CONFIGURATION_AP_PASSWORD_MIN_LENGTH) {
		com_return_error(data, sizeof(MessageHeader), MESSAGE_ERROR_CODE_INVALID_PARAMETER, cid);
		return;
	}

	os_memcpy(configuration_current_to_save.ap_password, data->password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	com_return_setter(cid, data);
}

void ICACHE_FLASH_ATTR get_wifi2_ap_password(const int8_t cid, const GetWifi2APPassword *data) {
	GetWifi2APPasswordReturn gw2appr;

	gw2appr.header        = data->header;
	gw2appr.header.length = sizeof(GetWifi2APPasswordReturn);
	os_memset(gw2appr.password, '\0', CONFIGURATION_PASSWORD_MAX_LENGTH);

	com_send(&gw2appr, sizeof(GetWifi2APPasswordReturn), cid);
}

void ICACHE_FLASH_ATTR save_wifi2_configuration(const int8_t cid, const SaveWifi2Configuration *data) {
	SaveWifi2ConfigurationReturn sw2cr;

	/*
	 * When mesh mode is enabled client and AP mode must be disbaled.
	 * If a situation is detected where mesh mode is enabled along with client/AP
	 * mode then disable mesh mode.
	 *
	 * This situation can occur if a brickv older than version 2.3.7 is being used
	 * to configure a master and WIFI extension 2 with newer firmware which already
	 * had mesh enabled or if the user is calling these APIs and isn't disabling
	 * mesh mode and is enabling either or both of client/AP mode.
	 */
	if(configuration_current.mesh_enable) {
		if(configuration_current_to_save.client_enable || configuration_current_to_save.ap_enable) {
			configuration_current_to_save.mesh_enable = false;
		}
	}

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
