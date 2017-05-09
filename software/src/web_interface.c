/* WIFI Extension 2.0
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * web_interface.c: Web interface for WIFI Extension
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

#include <string.h>
#include "espmissingincludes.h"
#include <esp8266.h>
#include <jsonparse.h>
#include "httpdespfs.h"
#include "web_interface.h"
#include "configuration.h"
#include "communication.h"
#include "tfp_connection.h"

// Defines.
#define JSON_REQUEST_AUTHENTICATE 1
#define JSON_REQUEST_GET_STATUS 2
#define JSON_REQUEST_UPDATE_SETTINGS 3
#define JSON_REQUEST_END_SESSION 4
#define JSON_REQUEST_IS_ALREADY_AUTHENTICATED 5
#define JSON_REQUEST_GET_SETTINGS 6
#define JSON_REQUEST_INIT_AUTHENTICATION 7
#define JSON_STATUS_OK 1
#define JSON_STATUS_FAILED 2
#define HEADER_FIELD_TYPE_STRING 1
#define HEADER_FIELD_TYPE_NUMERIC 2
#define GENERIC_BUFFER_SIZE 1024
#define POST_BUFFER_SIZE 2048
#define MAX_ACTIVE_SESSION_COOKIES 8
#define MAX_AUTHENTICATION_SLOTS 8
#define JSON_PARSE_STATE_SLOT 1
#define JSON_PARSE_STATE_BYTES_RN_CLIENT 2
#define JSON_PARSE_STATE_SHA1_HMAC_CLIENT 3

// String formats.
const char *fmt_response_json = "{\"request\":%d,\"status\":%d,\"data\":%s}";

const char *fmt_response_json_status_data = \
	"{\"operating_mode\":%d, \
\"client_status\":\"%s\", \
\"signal_strength\":%d, \
\"client_ip\":\"%s\", \
\"client_netmask\":\"%s\", \
\"client_gateway\":\"%s\", \
\"client_mac\":\"%s\", \
\"client_rx_count\":%d, \
\"client_tx_count\":%d, \
\"ap_connected_clients\":%d, \
\"ap_ip\":\"%s\", \
\"ap_netmask\":\"%s\", \
\"ap_gateway\":\"%s\", \
\"ap_mac\":\"%s\", \
\"ap_rx_count\":%d, \
\"ap_tx_count\":%d\
}";

const char *fmt_response_json_settings_data = \
	"{\"general_port\":%d, \
\"general_websocket_port\":%d, \
\"general_website_port\":%d, \
\"general_phy_mode\":%d, \
\"general_sleep_mode\":%d, \
\"general_website\":%d, \
\"general_authentication_secret\":\"%s\", \
\"client_enable\":%d, \
\"client_ssid\":\"%s\", \
\"client_ip\":[%d,%d,%d,%d], \
\"client_subnet_mask\":[%d,%d,%d,%d], \
\"client_gateway\":[%d,%d,%d,%d], \
\"client_mac_address\":[%d,%d,%d,%d,%d,%d], \
\"client_bssid\":[%d,%d,%d,%d,%d,%d], \
\"client_hostname\":\"%s\", \
\"ap_enable\":%d, \
\"ap_ssid\":\"%s\", \
\"ap_ip\":[%d,%d,%d,%d], \
\"ap_subnet_mask\":[%d,%d,%d,%d], \
\"ap_gateway\":[%d,%d,%d,%d], \
\"ap_encryption\":%d, \
\"ap_hidden\":%d, \
\"ap_mac_address\":[%d,%d,%d,%d,%d,%d], \
\"ap_channel\":%d, \
\"ap_hostname\":\"%s\"\
}";

const char *fmt_set_session_cookie = "sid=%lu";

// Variables
int index_buffer_post_form = 0;
extern GetWifi2StatusReturn gw2sr;
char buffer_post_form[POST_BUFFER_SIZE];
uint8_t current_session_cookie_index = 0;
extern Configuration configuration_current;
extern Configuration configuration_current_to_save;
uint8_t current_authentication_slot_index = 0;
unsigned long active_sessions[MAX_ACTIVE_SESSION_COOKIES];
unsigned long authentication_slots[MAX_AUTHENTICATION_SLOTS];

// httpdFindArg (actually httpdUrlDecode) only NULL-terminates the string in the
// buffer if there is free space in the buffer to do so. find_arg ensures that
// the string in the buffer is always NULL-terminated
int ICACHE_FLASH_ATTR find_arg(char *line, char *arg, char *buff, int buffLen) {
	if (buffLen < 1) {
		return 0;
	}

	int result = httpdFindArg(line, arg, buff, buffLen - 1);

	buff[buffLen - 1] = '\0';

	return result;
}

int ICACHE_FLASH_ATTR do_get_sid(unsigned long *sid) {
	do{
		*sid = os_random();
	}
	while(*sid <= 0);

	active_sessions[current_session_cookie_index] = *sid;

	if(current_session_cookie_index == MAX_ACTIVE_SESSION_COOKIES - 1){
		current_session_cookie_index = 0;
	}
	else {
		current_session_cookie_index++;
	}

	return 1;
}

int ICACHE_FLASH_ATTR do_is_auth_enabled(void) {
	if(configuration_current_to_save.general_authentication_secret[0] == '\0')
		return -1;

	return 1;
}

int ICACHE_FLASH_ATTR do_get_cookie_field(char *cookie,
										char *field,
										unsigned char type_field,
										void *value) {
	char *token;
	char _cookie[GENERIC_BUFFER_SIZE + 1]; // +1 for NULL-terminator

	if(!strstr(cookie, field)) {
		return -1;
	}

	strncpy(_cookie, cookie, GENERIC_BUFFER_SIZE);
	_cookie[GENERIC_BUFFER_SIZE] = '\0';

	token = strtok(_cookie, ";");

	while(token) {
		if(strstr(token, field)) {
			strcpy(_cookie, token);
			token = strtok(_cookie, "=");
			token = strtok(NULL, "=");

			switch(type_field) {
				case HEADER_FIELD_TYPE_NUMERIC:
					*((unsigned long*)value) = strtoul(token, NULL, 10);
					break;
				case HEADER_FIELD_TYPE_STRING:
					strcpy((char *)value, token);
					break;
				default:
					return -1;
			}

			return 1;
		}

		token = strtok(NULL, ";");
	}

	return -1;
}

int ICACHE_FLASH_ATTR do_has_cookie(HttpdConnData *connection_data,
								 	char *cookie,
								 	unsigned long length) {
	if((httpdGetHeader(connection_data,
					   "Cookie",
					   cookie,
					   length)) == 1) {
		return 1;
	}

	else {
		return -1;
	}
}

int ICACHE_FLASH_ATTR cgi_get_status(HttpdConnData *connection_data) {
	struct get_status status;
	char response[GENERIC_BUFFER_SIZE];
	char response_status_data[GENERIC_BUFFER_SIZE];

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(connection_data->post->buff,
						 JSON_REQUEST_GET_STATUS)) == 1)) {
		do_get_status(&status, 0);

	 	sprintf(response_status_data,
				fmt_response_json_status_data,
				status.operating_mode,
				status.client_status,
				status.signal_strength,
				status.client_ip,
				status.client_netmask,
				status.client_gateway,
				status.client_mac,
				status.client_rx_count,
				status.client_tx_count,
				status.ap_connected_clients,
				status.ap_ip,
				status.ap_netmask,
				status.ap_gateway,
				status.ap_mac,
				status.ap_rx_count,
				status.ap_tx_count);

		sprintf(response,
				fmt_response_json,
				JSON_REQUEST_GET_STATUS,
				JSON_STATUS_OK,
				response_status_data);
	}
	else {
		do_get_status(&status, 1);

		sprintf(response_status_data,
				fmt_response_json_status_data,
				status.operating_mode,
				status.client_status,
				status.signal_strength,
				status.client_ip,
				status.client_netmask,
				status.client_gateway,
				status.client_mac,
				status.client_rx_count,
				status.client_tx_count,
				status.ap_connected_clients,
				status.ap_ip,
				status.ap_netmask,
				status.ap_gateway,
				status.ap_mac,
				status.ap_rx_count,
				status.ap_tx_count);

		sprintf(response,
				fmt_response_json,
				JSON_REQUEST_GET_STATUS,
				JSON_STATUS_FAILED,
				response_status_data);
	}

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_404(HttpdConnData *connection_data) {
	connection_data->url = "/not_found.html";

	return cgiEspFsHook(connection_data);
}

int ICACHE_FLASH_ATTR cgi_root(HttpdConnData *connection_data) {
	if((do_check_session(connection_data)) == 1) {
		connection_data->url = "/index.html";

		return cgiEspFsHook(connection_data);
	}

	connection_data->cgiArg = "/authenticate.html";

	return cgiRedirect(connection_data);
}

int ICACHE_FLASH_ATTR do_check_request(char *buffer_post, uint8 rid) {
	char request[4];

	if ((find_arg(buffer_post, "request", request, 4)) > 0) {
		if((uint8)(strtoul(request, NULL, 10)) == rid) {
			return 1;
		}
	}

	return -1;
}

int ICACHE_FLASH_ATTR do_check_session(HttpdConnData *connection_data) {
	unsigned long sid;
	char cookie[GENERIC_BUFFER_SIZE];

	if((do_is_auth_enabled()) == -1) {
		return 1;
	}

	if((do_has_cookie(connection_data,
					  cookie,
					  GENERIC_BUFFER_SIZE)) == 1) {

		if((do_get_cookie_field(cookie,
								"sid=",
								HEADER_FIELD_TYPE_NUMERIC,
								(void *)&sid)) == 1) {
			for(unsigned long i = 0; i < MAX_ACTIVE_SESSION_COOKIES; i++) {
				if(sid == active_sessions[i]) {
					return 1;
				}
			}
		}
	}

	return -1;
}

int ICACHE_FLASH_ATTR do_get_status(struct get_status *status,
									unsigned char init_only) {
	char ap_mac[6];
	char mac_byte[3];
	struct ip_info ip;
	uint8 op_mode = 0;
	char client_mac[6];

	status->operating_mode = 0;
	status->signal_strength = 0;
	status->ap_connected_clients = 0;
	status->ap_rx_count = gw2sr.ap_rx_count;
	status->ap_tx_count = gw2sr.ap_tx_count;
	status->client_rx_count = gw2sr.client_rx_count;
	status->client_tx_count = gw2sr.client_tx_count;

	strcpy(status->client_status, "-1");
	strcpy(status->client_ip, "-");
	strcpy(status->client_netmask, "-");
	strcpy(status->client_gateway, "-");
	strcpy(status->client_mac, "");
	strcpy(status->ap_ip, "-");
	strcpy(status->ap_netmask, "-");
	strcpy(status->ap_gateway, "-");
	strcpy(status->ap_mac, "");

	if(init_only) {
		strcpy(status->client_mac, "-");
		strcpy(status->ap_mac, "-");

		return 1;
	}

	// Operating mode.
	op_mode = wifi_get_opmode();

	if(op_mode > 0)
		status->operating_mode = wifi_get_opmode();
	else
		return 1;

	if(status->operating_mode == STATION_MODE ||
	   status->operating_mode == STATIONAP_MODE) {
			// Wifi client status.
			switch(wifi_station_get_connect_status()) {
				case STATION_IDLE:
					strcpy(status->client_status, "Idle");
					break;
		    	case STATION_CONNECTING:
					strcpy(status->client_status, "Connecting...");
					break;
		    	case STATION_WRONG_PASSWORD:
					strcpy(status->client_status, "Wrong Password");
					break;
		    	case STATION_NO_AP_FOUND:
					strcpy(status->client_status, "No AP Found");
					break;
		    	case STATION_CONNECT_FAIL:
					strcpy(status->client_status, "Connect Failed");
					break;
		    	case STATION_GOT_IP:
					strcpy(status->client_status, "Got IP");
					break;
		    	default:
		    		strcpy(status->client_status, "-");
		    		break;
			}

			// Client signal strength.
			sint8 client_rssi = wifi_station_get_rssi();

			if(client_rssi != 31)
				status->signal_strength = (uint8)(2 * (client_rssi + 100));
			else
				status->signal_strength = 0;

			// Client IP info.
			if(wifi_get_ip_info(STATION_IF, &ip)) {
				sprintf(status->client_ip, IPSTR, IP2STR(&ip.ip));
				sprintf(status->client_netmask, IPSTR, IP2STR(&ip.netmask));
				sprintf(status->client_gateway, IPSTR, IP2STR(&ip.gw));
			}

			// Client MAC.
			if(wifi_get_macaddr(STATION_IF, client_mac)) {
				for(int i = 0; i < 6; i++) {
					if(i < 5)
						sprintf(mac_byte, "%x:", client_mac[i]);
					else
						sprintf(mac_byte, "%x", client_mac[i]);
					strcat(status->client_mac, mac_byte);
				}
			}
			else
				strcpy(status->client_mac, "-");
	}

	if(status->operating_mode == SOFTAP_MODE ||
	   status->operating_mode == STATIONAP_MODE) {
			// AP IP info.
			if(wifi_get_ip_info(SOFTAP_IF, &ip)) {
				sprintf(status->ap_ip, IPSTR, IP2STR(&ip.ip));
				sprintf(status->ap_netmask, IPSTR, IP2STR(&ip.netmask));
				sprintf(status->ap_gateway, IPSTR, IP2STR(&ip.gw));
			}

			// AP MAC.
			if(wifi_get_macaddr(SOFTAP_IF, ap_mac)) {
				for(int i = 0; i < 6; i++) {
					if(i < 5)
						sprintf(mac_byte, "%x:", ap_mac[i]);
					else
						sprintf(mac_byte, "%x", ap_mac[i]);
					strcat(status->ap_mac, mac_byte);
				}
			}
			else
				strcpy(status->ap_mac, "-");

			// AP connected clients.
			status->ap_connected_clients = wifi_softap_get_station_num();
	}

	return 1;
}

int ICACHE_FLASH_ATTR cgi_end_session(HttpdConnData *connection_data) {
	unsigned long sid;
	char cookie[GENERIC_BUFFER_SIZE];
	char response[GENERIC_BUFFER_SIZE];

	httpdStartResponse(connection_data, 200);

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(connection_data->post->buff,
						  JSON_REQUEST_END_SESSION)) == 1)) {
			if((do_has_cookie(connection_data,
							  cookie,
							  GENERIC_BUFFER_SIZE)) == 1) {
				if((do_get_cookie_field(cookie,
										"sid=",
										HEADER_FIELD_TYPE_NUMERIC,
										(void *)&sid)) == 1) {
					for(unsigned long i = 0; i < MAX_ACTIVE_SESSION_COOKIES; i++) {
						if(sid == active_sessions[i]) {
							active_sessions[i] = 0;
						}
					}
					sprintf(response,
							fmt_response_json,
							JSON_REQUEST_END_SESSION,
							JSON_STATUS_OK,
							"null");

					httpdEndHeaders(connection_data);
					httpdSend(connection_data, response, -1);

					return HTTPD_CGI_DONE;
				}
			}
	}

	sprintf(response,
			fmt_response_json,
			JSON_REQUEST_END_SESSION,
			JSON_STATUS_FAILED,
			"null");

	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_authenticate(HttpdConnData *connection_data) {
	uint8_t slot;
	int json_type;
	unsigned long sid;
	char parsed_key[64];
	uint8_t bytes_rn_client[4];
	char data[GENERIC_BUFFER_SIZE];
	uint8_t json_parse_current_state;
	struct jsonparse_state json_state;
	char response[GENERIC_BUFFER_SIZE];
	uint8_t index_bytes_rn_client = 0;
	uint8_t bytes_sha1_hmac_client[20];
	uint8_t index_bytes_sha1_hmac_client = 0;
	char header_set_cookie[GENERIC_BUFFER_SIZE];

	httpdStartResponse(connection_data, 200);

	if(((do_check_request(connection_data->post->buff,
						 JSON_REQUEST_AUTHENTICATE)) == 1) &&
		((find_arg(connection_data->post->buff,
				 		"data",
				 		data,
				 		GENERIC_BUFFER_SIZE)) > 0)) {
			// Sample data received from client.
			/*
			{
				"slot":1,
				"bytes_rn_client":[65,198,4,42],
				"sha1_hmac_client":[7, 152, 199, 132, 100, 14, 26, 10, 169, 118,
														228, 221, 254, 145, 51, 174, 124, 219, 77, 183]
			}
			*/
			jsonparse_setup(&json_state, data, strlen(data));

			while(json_type = jsonparse_next(&json_state))
		  {
		    switch (json_type)
		    {
		      case JSON_TYPE_ARRAY:
		        break;

		      case JSON_TYPE_OBJECT:
		        break;

		      case JSON_TYPE_PAIR:
		        break;

		      case JSON_TYPE_PAIR_NAME:
					{
						// Update JSON parse state here.
		        jsonparse_copy_value(&json_state, parsed_key, 64);

						if(strcmp(parsed_key, "slot") == 0){
							json_parse_current_state = JSON_PARSE_STATE_SLOT;
						}
						else if(strcmp(parsed_key, "bytes_rn_client") == 0){
							json_parse_current_state = JSON_PARSE_STATE_BYTES_RN_CLIENT;
						}
						else if(strcmp(parsed_key, "sha1_hmac_client") == 0){
							json_parse_current_state = JSON_PARSE_STATE_SHA1_HMAC_CLIENT;
						}

		        break;
					}

		      case JSON_TYPE_STRING:
		        break;

		      case JSON_TYPE_INT:
					{
						if(json_parse_current_state == JSON_PARSE_STATE_SLOT){
							slot = (uint8_t)jsonparse_get_value_as_int(&json_state);
						}
						else if(json_parse_current_state == JSON_PARSE_STATE_BYTES_RN_CLIENT){
							bytes_rn_client[index_bytes_rn_client] =
							(uint8_t)jsonparse_get_value_as_int(&json_state);
							index_bytes_rn_client++;
						}
						else if(json_parse_current_state == JSON_PARSE_STATE_SHA1_HMAC_CLIENT){
							bytes_sha1_hmac_client[index_bytes_sha1_hmac_client] =
							(uint8_t)jsonparse_get_value_as_int(&json_state);
							index_bytes_sha1_hmac_client++;
						}

		        break;
					}

		      case JSON_TYPE_NUMBER:
					{
						if(json_parse_current_state == JSON_PARSE_STATE_SLOT){
							slot = (uint8_t)jsonparse_get_value_as_long(&json_state);
						}
						else if(json_parse_current_state == JSON_PARSE_STATE_BYTES_RN_CLIENT){
							bytes_rn_client[index_bytes_rn_client] =
							(uint8_t)jsonparse_get_value_as_long(&json_state);
							index_bytes_rn_client++;
						}
						else if(json_parse_current_state == JSON_PARSE_STATE_SHA1_HMAC_CLIENT){
							bytes_sha1_hmac_client[index_bytes_sha1_hmac_client] =
							(uint8_t)jsonparse_get_value_as_long(&json_state);
							index_bytes_sha1_hmac_client++;
						}

		        break;
					}

		      case JSON_TYPE_ERROR:
		        break;

		      default:
		        break;
		    }
		  }

			if(do_check_secret(slot, bytes_rn_client, bytes_sha1_hmac_client) == 1){
				if((do_get_sid(&sid)) == 1) {
					sprintf(header_set_cookie,
							fmt_set_session_cookie,
							sid);

					httpdHeader(connection_data, "Set-Cookie", header_set_cookie);
					httpdEndHeaders(connection_data);

					sprintf(response,
							fmt_response_json,
							JSON_REQUEST_AUTHENTICATE,
							JSON_STATUS_OK,
							"null");

					httpdSend(connection_data, response, -1);

					return HTTPD_CGI_DONE;
				}
			}
	}

	sprintf(response,
			fmt_response_json,
			JSON_REQUEST_AUTHENTICATE,
			JSON_STATUS_FAILED,
			"null");

	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_get_settings(HttpdConnData *connection_data) {
	char response[GENERIC_BUFFER_SIZE];
	char response_settings[GENERIC_BUFFER_SIZE];
	char general_authentication_secret[CONFIGURATION_SECRET_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char client_ssid[CONFIGURATION_SSID_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char client_hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char ap_ssid[CONFIGURATION_SSID_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char ap_hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(connection_data->post->buff, JSON_REQUEST_GET_SETTINGS)) == 1)) {

		// ensure that those config strings are NULL-terminated
		os_memcpy(general_authentication_secret, configuration_current_to_save.general_authentication_secret, CONFIGURATION_SECRET_MAX_LENGTH);
		os_memcpy(client_ssid, configuration_current_to_save.client_ssid, CONFIGURATION_SSID_MAX_LENGTH);
		os_memcpy(client_hostname, configuration_current_to_save.client_hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);
		os_memcpy(ap_ssid, configuration_current_to_save.ap_ssid, CONFIGURATION_SSID_MAX_LENGTH);
		os_memcpy(ap_hostname, configuration_current_to_save.ap_hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);

	 	sprintf(response_settings,
		        fmt_response_json_settings_data,
		        configuration_current_to_save.general_port,
		        configuration_current_to_save.general_websocket_port,
		        configuration_current_to_save.general_website_port,
		        configuration_current_to_save.general_phy_mode,
		        configuration_current_to_save.general_sleep_mode,
		        configuration_current_to_save.general_website,
		        general_authentication_secret,
		        configuration_current_to_save.client_enable,
		        client_ssid,
		        configuration_current_to_save.client_ip[0],
		        configuration_current_to_save.client_ip[1],
		        configuration_current_to_save.client_ip[2],
		        configuration_current_to_save.client_ip[3],
		        configuration_current_to_save.client_subnet_mask[0],
		        configuration_current_to_save.client_subnet_mask[1],
		        configuration_current_to_save.client_subnet_mask[2],
		        configuration_current_to_save.client_subnet_mask[3],
		        configuration_current_to_save.client_gateway[0],
		        configuration_current_to_save.client_gateway[1],
		        configuration_current_to_save.client_gateway[2],
		        configuration_current_to_save.client_gateway[3],
		        configuration_current_to_save.client_mac_address[0],
		        configuration_current_to_save.client_mac_address[1],
		        configuration_current_to_save.client_mac_address[2],
		        configuration_current_to_save.client_mac_address[3],
		        configuration_current_to_save.client_mac_address[4],
		        configuration_current_to_save.client_mac_address[5],
		        configuration_current_to_save.client_bssid[0],
		        configuration_current_to_save.client_bssid[1],
		        configuration_current_to_save.client_bssid[2],
		        configuration_current_to_save.client_bssid[3],
		        configuration_current_to_save.client_bssid[4],
		        configuration_current_to_save.client_bssid[5],
		        client_hostname,
		        configuration_current_to_save.ap_enable,
		        ap_ssid,
		        configuration_current_to_save.ap_ip[0],
		        configuration_current_to_save.ap_ip[1],
		        configuration_current_to_save.ap_ip[2],
		        configuration_current_to_save.ap_ip[3],
		        configuration_current_to_save.ap_subnet_mask[0],
		        configuration_current_to_save.ap_subnet_mask[1],
		        configuration_current_to_save.ap_subnet_mask[2],
		        configuration_current_to_save.ap_subnet_mask[3],
		        configuration_current_to_save.ap_gateway[0],
		        configuration_current_to_save.ap_gateway[1],
		        configuration_current_to_save.ap_gateway[2],
		        configuration_current_to_save.ap_gateway[3],
		        configuration_current_to_save.ap_encryption,
		        configuration_current_to_save.ap_hidden,
		        configuration_current_to_save.ap_mac_address[0],
		        configuration_current_to_save.ap_mac_address[1],
		        configuration_current_to_save.ap_mac_address[2],
		        configuration_current_to_save.ap_mac_address[3],
		        configuration_current_to_save.ap_mac_address[4],
		        configuration_current_to_save.ap_mac_address[5],
		        configuration_current_to_save.ap_channel,
		        ap_hostname);

		sprintf(response,
				fmt_response_json,
				JSON_REQUEST_GET_SETTINGS,
				JSON_STATUS_OK,
				response_settings);
	}
	else {
		sprintf(response,
				fmt_response_json,
				JSON_REQUEST_GET_SETTINGS,
				JSON_STATUS_FAILED,
				"null");
	}

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR do_initialize_web_interface_session_tracking(void) {
	for(int i = 0; i < MAX_ACTIVE_SESSION_COOKIES; i++) {
		active_sessions[i] = 0;
	}

	return 1;
}

int ICACHE_FLASH_ATTR do_update_settings_ap(char *data) {
	// AP fields.
	char ap_ip_configuration[2];
	char ap_static_ip_0[4];
	char ap_static_ip_1[4];
	char ap_static_ip_2[4];
	char ap_static_ip_3[4];
	char ap_static_netmask_0[4];
	char ap_static_netmask_1[4];
	char ap_static_netmask_2[4];
	char ap_static_netmask_3[4];
	char ap_static_gateway_0[4];
	char ap_static_gateway_1[4];
	char ap_static_gateway_2[4];
	char ap_static_gateway_3[4];
	char ap_ssid[CONFIGURATION_SSID_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char ap_encryption[2];
	char ap_password[CONFIGURATION_PASSWORD_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char ap_channel[3];
	char ap_hide_ssid[2];
	char ap_mac_address[2];
	char ap_mac_address_0[3];
	char ap_mac_address_1[3];
	char ap_mac_address_2[3];
	char ap_mac_address_3[3];
	char ap_mac_address_4[3];
	char ap_mac_address_5[3];
	int ret = 0;

	if((find_arg(data, "ap_ip_configuration", ap_ip_configuration, 2)) > 0) {
		// DHCP.
		if(strtoul(ap_ip_configuration, NULL, 10) == 0){
			os_memset(configuration_current_to_save.ap_ip, 0, 4);
			os_memset(configuration_current_to_save.ap_subnet_mask, 0, 4);
			os_memset(configuration_current_to_save.ap_gateway, 0, 4);
		}
		// Static.
		else if(strtoul(ap_ip_configuration, NULL, 10) == 1){
			if(((find_arg(data, "ap_static_ip_0", ap_static_ip_0, 4)) > 0) &&
			   ((find_arg(data, "ap_static_ip_1", ap_static_ip_1, 4)) > 0) &&
			   ((find_arg(data, "ap_static_ip_2", ap_static_ip_2, 4)) > 0) &&
			   ((find_arg(data, "ap_static_ip_3", ap_static_ip_3, 4)) > 0) &&
			   ((find_arg(data, "ap_static_netmask_0", ap_static_netmask_0, 4)) > 0) &&
			   ((find_arg(data, "ap_static_netmask_1", ap_static_netmask_1, 4)) > 0) &&
			   ((find_arg(data, "ap_static_netmask_2", ap_static_netmask_2, 4)) > 0) &&
			   ((find_arg(data, "ap_static_netmask_3", ap_static_netmask_3, 4)) > 0) &&
			   ((find_arg(data, "ap_static_gateway_0", ap_static_gateway_0, 4)) > 0) &&
			   ((find_arg(data, "ap_static_gateway_1", ap_static_gateway_1, 4)) > 0) &&
			   ((find_arg(data, "ap_static_gateway_2", ap_static_gateway_2, 4)) > 0) &&
			   ((find_arg(data, "ap_static_gateway_3", ap_static_gateway_3, 4)) > 0)) {
				configuration_current_to_save.ap_ip[0] = (uint8_t)strtoul(ap_static_ip_0, NULL, 10);
				configuration_current_to_save.ap_ip[1] = (uint8_t)strtoul(ap_static_ip_1, NULL, 10);
				configuration_current_to_save.ap_ip[2] = (uint8_t)strtoul(ap_static_ip_2, NULL, 10);
				configuration_current_to_save.ap_ip[3] = (uint8_t)strtoul(ap_static_ip_3, NULL, 10);
				configuration_current_to_save.ap_subnet_mask[0] = (uint8_t)strtoul(ap_static_netmask_0, NULL, 10);
				configuration_current_to_save.ap_subnet_mask[1] = (uint8_t)strtoul(ap_static_netmask_1, NULL, 10);
				configuration_current_to_save.ap_subnet_mask[2] = (uint8_t)strtoul(ap_static_netmask_2, NULL, 10);
				configuration_current_to_save.ap_subnet_mask[3] = (uint8_t)strtoul(ap_static_netmask_3, NULL, 10);
				configuration_current_to_save.ap_gateway[0] = (uint8_t)strtoul(ap_static_gateway_0, NULL, 10);
				configuration_current_to_save.ap_gateway[1] = (uint8_t)strtoul(ap_static_gateway_1, NULL, 10);
				configuration_current_to_save.ap_gateway[2] = (uint8_t)strtoul(ap_static_gateway_2, NULL, 10);
				configuration_current_to_save.ap_gateway[3] = (uint8_t)strtoul(ap_static_gateway_3, NULL, 10);
			}
		}
	}

	// AP SSID.
	if((find_arg(data, "ap_ssid", ap_ssid, CONFIGURATION_SSID_MAX_LENGTH + 1)) > 0) { // +1 for NULL-terminator
		os_memcpy(configuration_current_to_save.ap_ssid, ap_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	}
	else{
		os_memset(configuration_current_to_save.ap_ssid, 0, CONFIGURATION_SSID_MAX_LENGTH);
	}

	// AP Password
	ret = find_arg(data, "ap_password", ap_password,
								 CONFIGURATION_PASSWORD_MAX_LENGTH + 1); // +1 for NULL-terminator

	if(ret > 0) {
		os_memcpy(configuration_current_to_save.ap_password, ap_password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	}
	else if(ret == 0) {
		os_memset(configuration_current_to_save.ap_password, 0, CONFIGURATION_PASSWORD_MAX_LENGTH);
	}

	// AP Encryption.
	ret = find_arg(data, "ap_encryption", ap_encryption, 2);

	if(ret > 0) {
		configuration_current_to_save.ap_encryption = (uint8_t)strtoul(ap_encryption, NULL,  10);
	}
	else if(ret == 0) {
		configuration_current_to_save.ap_encryption = 0;
	}

	// AP Channel.
	if((find_arg(data, "ap_channel", ap_channel, 3)) > 0) {
		configuration_current_to_save.ap_channel = (uint8_t)strtoul(ap_channel, NULL, 10);
	}

	// Hide AP SSID.
	if((find_arg(data, "ap_hide_ssid", ap_hide_ssid, 2)) > 0) {
		if(strtoul(ap_hide_ssid, NULL, 10) == 0) {
			configuration_current_to_save.ap_hidden = 0;
		}
		else if(strtoul(ap_hide_ssid, NULL, 10) == 1){
			configuration_current_to_save.ap_hidden = 1;
		}
	}

	// AP MAC.
	if((find_arg(data, "ap_mac_address", ap_mac_address, 2)) > 0) {
		if(strtoul(ap_mac_address, NULL, 10) == 0){
			os_memset(configuration_current_to_save.ap_mac_address, 0, 6);
		}
		else if(strtoul(ap_mac_address, NULL, 10) == 1){
			if(((find_arg(data, "ap_mac_address_0", ap_mac_address_0, 3)) > 0) &&
			   ((find_arg(data, "ap_mac_address_1", ap_mac_address_1, 3)) > 0) &&
			   ((find_arg(data, "ap_mac_address_2", ap_mac_address_2, 3)) > 0) &&
			   ((find_arg(data, "ap_mac_address_3", ap_mac_address_3, 3)) > 0) &&
			   ((find_arg(data, "ap_mac_address_4", ap_mac_address_4, 3)) > 0) &&
			   ((find_arg(data, "ap_mac_address_5", ap_mac_address_5, 3)) > 0)) {
					configuration_current_to_save.ap_mac_address[0] = (uint8_t)strtoul(ap_mac_address_0, NULL, 16);
					configuration_current_to_save.ap_mac_address[1] = (uint8_t)strtoul(ap_mac_address_1, NULL, 16);
					configuration_current_to_save.ap_mac_address[2] = (uint8_t)strtoul(ap_mac_address_2, NULL, 16);
					configuration_current_to_save.ap_mac_address[3] = (uint8_t)strtoul(ap_mac_address_3, NULL, 16);
					configuration_current_to_save.ap_mac_address[4] = (uint8_t)strtoul(ap_mac_address_4, NULL, 16);
					configuration_current_to_save.ap_mac_address[5] = (uint8_t)strtoul(ap_mac_address_5, NULL, 16);
			}
		}
	}

	return 1;
}

int ICACHE_FLASH_ATTR do_update_settings_client(char *data) {
	// Client fields.
	char client_hostname[CONFIGURATION_HOSTNAME_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char client_ip_configuration[2];
	char client_static_ip_0[4];
	char client_static_ip_1[4];
	char client_static_ip_2[4];
	char client_static_ip_3[4];
	char client_static_netmask_0[4];
	char client_static_netmask_1[4];
	char client_static_netmask_2[4];
	char client_static_netmask_3[4];
	char client_static_gateway_0[4];
	char client_static_gateway_1[4];
	char client_static_gateway_2[4];
	char client_static_gateway_3[4];
	char client_ssid[CONFIGURATION_SSID_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char client_password[CONFIGURATION_PASSWORD_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char client_connect_to_ap_with_specific_bssid[2];
	char client_bssid_0[3];
	char client_bssid_1[3];
	char client_bssid_2[3];
	char client_bssid_3[3];
	char client_bssid_4[3];
	char client_bssid_5[3];
	char client_mac_address[2];
	char client_mac_address_0[3];
	char client_mac_address_1[3];
	char client_mac_address_2[3];
	char client_mac_address_3[3];
	char client_mac_address_4[3];
	char client_mac_address_5[3];
	int ret = 0;

	// Hostname in client mode.
	if((find_arg(data, "client_hostname", client_hostname,
	             CONFIGURATION_HOSTNAME_MAX_LENGTH + 1)) > 0) { // +1 for NULL-terminator
		os_memcpy(configuration_current_to_save.client_hostname, client_hostname, CONFIGURATION_HOSTNAME_MAX_LENGTH);
	}
	else{
		os_memset(configuration_current_to_save.client_hostname, 0, CONFIGURATION_HOSTNAME_MAX_LENGTH);
	}

	// Client mode IP configuration.
	if((find_arg(data, "client_ip_configuration", client_ip_configuration, 2)) > 0) {
		// DHCP.
		if(strtoul(client_ip_configuration, NULL, 10) == 0) {
			os_memset(configuration_current_to_save.client_ip, 0,  4);
			os_memset(configuration_current_to_save.client_subnet_mask, 0, 4);
			os_memset(configuration_current_to_save.client_gateway, 0, 4);
		}
		// Static.
		else if(strtoul(client_ip_configuration, NULL, 10) == 1) {
			if(((find_arg(data, "client_static_ip_0", client_static_ip_0, 4)) > 0) &&
			   ((find_arg(data, "client_static_ip_1", client_static_ip_1, 4)) > 0) &&
			   ((find_arg(data, "client_static_ip_2", client_static_ip_2, 4)) > 0) &&
			   ((find_arg(data, "client_static_ip_3", client_static_ip_3, 4)) > 0) &&
			   ((find_arg(data, "client_static_netmask_0", client_static_netmask_0, 4)) > 0) &&
			   ((find_arg(data, "client_static_netmask_1", client_static_netmask_1, 4)) > 0) &&
			   ((find_arg(data, "client_static_netmask_2", client_static_netmask_2, 4)) > 0) &&
			   ((find_arg(data, "client_static_netmask_3", client_static_netmask_3, 4)) > 0) &&
			   ((find_arg(data, "client_static_gateway_0", client_static_gateway_0, 4)) > 0) &&
			   ((find_arg(data, "client_static_gateway_1", client_static_gateway_1, 4)) > 0) &&
			   ((find_arg(data, "client_static_gateway_2", client_static_gateway_2, 4)) > 0) &&
			   ((find_arg(data, "client_static_gateway_3", client_static_gateway_3, 4)) > 0)) {
				configuration_current_to_save.client_ip[0] = (uint8_t)strtoul(client_static_ip_0, NULL, 10);
				configuration_current_to_save.client_ip[1] = (uint8_t)strtoul(client_static_ip_1, NULL, 10);
				configuration_current_to_save.client_ip[2] = (uint8_t)strtoul(client_static_ip_2, NULL, 10);
				configuration_current_to_save.client_ip[3] = (uint8_t)strtoul(client_static_ip_3, NULL, 10);
				configuration_current_to_save.client_subnet_mask[0] = (uint8_t)strtoul(client_static_netmask_0, NULL, 10);
				configuration_current_to_save.client_subnet_mask[1] = (uint8_t)strtoul(client_static_netmask_1, NULL, 10);
				configuration_current_to_save.client_subnet_mask[2] = (uint8_t)strtoul(client_static_netmask_2, NULL, 10);
				configuration_current_to_save.client_subnet_mask[3] = (uint8_t)strtoul(client_static_netmask_3, NULL, 10);
				configuration_current_to_save.client_gateway[0] = (uint8_t)strtoul(client_static_gateway_0, NULL, 10);
				configuration_current_to_save.client_gateway[1] = (uint8_t)strtoul(client_static_gateway_1, NULL, 10);
				configuration_current_to_save.client_gateway[2] = (uint8_t)strtoul(client_static_gateway_2, NULL, 10);
				configuration_current_to_save.client_gateway[3] = (uint8_t)strtoul(client_static_gateway_3, NULL, 10);
			}
		}
	}

	// SSID to connect to in client mode.
	if((find_arg(data, "client_ssid", client_ssid,
	             CONFIGURATION_SSID_MAX_LENGTH + 1)) > 0) { // +1 for NULL-terminator
		os_memcpy(configuration_current_to_save.client_ssid, client_ssid, CONFIGURATION_SSID_MAX_LENGTH);
	}
	else {
		os_memset(configuration_current_to_save.client_ssid, 0, CONFIGURATION_SSID_MAX_LENGTH);
	}

	ret = find_arg(data, "client_password", client_password,
							   CONFIGURATION_PASSWORD_MAX_LENGTH + 1); // +1 for NULL-terminator

	// Encryption in client mode.
	if(ret > 0) {
		os_memcpy(configuration_current_to_save.client_password, client_password, CONFIGURATION_PASSWORD_MAX_LENGTH);
	}
	else if(ret == 0) {
		os_memset(configuration_current_to_save.client_password, 0, CONFIGURATION_PASSWORD_MAX_LENGTH);
	}

	// Connect to specific BSSID.
	if((find_arg(data, "client_connect_to_ap_with_specific_bssid",
	             client_connect_to_ap_with_specific_bssid, 2)) > 0) {
		if(strtoul(client_connect_to_ap_with_specific_bssid, NULL, 10) == 0) {
			os_memset(configuration_current_to_save.client_bssid, 0, 6);
		}
		else if(strtoul(client_connect_to_ap_with_specific_bssid, NULL, 10) == 1) {
			if(((find_arg(data, "client_bssid_0", client_bssid_0, 3)) > 0) &&
			   ((find_arg(data, "client_bssid_1", client_bssid_1, 3)) > 0) &&
			   ((find_arg(data, "client_bssid_2", client_bssid_2, 3)) > 0) &&
			   ((find_arg(data, "client_bssid_3", client_bssid_3, 3)) > 0) &&
			   ((find_arg(data, "client_bssid_4", client_bssid_4, 3)) > 0) &&
			   ((find_arg(data, "client_bssid_5", client_bssid_5, 3)) > 0)) {
					configuration_current_to_save.client_bssid[0] = (uint8_t)strtoul(client_bssid_0, NULL, 16);
					configuration_current_to_save.client_bssid[1] = (uint8_t)strtoul(client_bssid_1, NULL, 16);
					configuration_current_to_save.client_bssid[2] = (uint8_t)strtoul(client_bssid_2, NULL, 16);
					configuration_current_to_save.client_bssid[3] = (uint8_t)strtoul(client_bssid_3, NULL, 16);
					configuration_current_to_save.client_bssid[4] = (uint8_t)strtoul(client_bssid_4, NULL, 16);
					configuration_current_to_save.client_bssid[5] = (uint8_t)strtoul(client_bssid_5, NULL, 16);
			}
		}
	}

	// Custom MAC address to be used in client mode.
	if((find_arg(data, "client_mac_address", client_mac_address, 2)) > 0) {
		if(strtoul(client_mac_address, NULL, 10) == 0) {
			os_memset(configuration_current_to_save.client_mac_address, 0, 6);
		}
		else if(strtoul(client_mac_address, NULL, 10) == 1) {
			if(((find_arg(data, "client_mac_address_0", client_mac_address_0, 3)) > 0) &&
			   ((find_arg(data, "client_mac_address_1", client_mac_address_1, 3)) > 0) &&
			   ((find_arg(data, "client_mac_address_2", client_mac_address_2, 3)) > 0) &&
			   ((find_arg(data, "client_mac_address_3", client_mac_address_3, 3)) > 0) &&
			   ((find_arg(data, "client_mac_address_4", client_mac_address_4, 3)) > 0) &&
			   ((find_arg(data, "client_mac_address_5", client_mac_address_5, 3)) > 0)) {
					configuration_current_to_save.client_mac_address[0] = (uint8_t)strtoul(client_mac_address_0, NULL, 16);
					configuration_current_to_save.client_mac_address[1] = (uint8_t)strtoul(client_mac_address_1, NULL, 16);
					configuration_current_to_save.client_mac_address[2] = (uint8_t)strtoul(client_mac_address_2, NULL, 16);
					configuration_current_to_save.client_mac_address[3] = (uint8_t)strtoul(client_mac_address_3, NULL, 16);
					configuration_current_to_save.client_mac_address[4] = (uint8_t)strtoul(client_mac_address_4, NULL, 16);
					configuration_current_to_save.client_mac_address[5] = (uint8_t)strtoul(client_mac_address_5, NULL, 16);
			}
		}
	}

	return 1;
}

int ICACHE_FLASH_ATTR cgi_update_settings(HttpdConnData *connection_data) {
	const uint8_t tf_reset_packet[8] = {0, 0, 0, 0, 8, 243, 0, 0};

	char data[POST_BUFFER_SIZE];

	// General fields.
	char general_port[6];
	char general_websocket_port[6];
	char general_website_port[6];
	char general_phy_mode[2];
	char general_use_authentication[2];
	char general_authentication_secret[CONFIGURATION_SECRET_MAX_LENGTH + 1] = {0}; // +1 for NULL-terminator
	char general_mode[2];

	// Partial arrival processing.
	os_memcpy(&buffer_post_form[index_buffer_post_form],
	          connection_data->post->buff,
	          connection_data->post->buffLen);

	index_buffer_post_form += connection_data->post->buffLen;

	// More pending data.
	if(connection_data->post->len != connection_data->post->received) {
		return HTTPD_CGI_MORE;
	}

	// Reset buffer accumulator index.
	index_buffer_post_form = 0;

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(buffer_post_form, JSON_REQUEST_UPDATE_SETTINGS)) == 1)) {
		if((find_arg(buffer_post_form, "data", data, POST_BUFFER_SIZE)) > 0) {

			// General settings.
			if((find_arg(data, "general_port", general_port, 6)) > 0) {
				configuration_current_to_save.general_port = (uint16_t)strtoul(general_port, NULL, 10);
			}
			if((find_arg(data, "general_websocket_port", general_websocket_port, 6)) > 0) {
				configuration_current_to_save.general_websocket_port = (uint16_t)strtoul(general_websocket_port, NULL, 10);
			}
			if((find_arg(data, "general_website_port", general_website_port, 6)) > 0) {
				configuration_current_to_save.general_website_port = (uint16_t)strtoul(general_website_port, NULL, 10);
			}
			if((find_arg(data, "general_phy_mode", general_phy_mode, 2)) > 0) {
				configuration_current_to_save.general_phy_mode = (uint8_t)strtoul(general_phy_mode, NULL, 10);
			}

			if((find_arg(data, "general_use_authentication", general_use_authentication, 2)) > 0) {
				if(((strtoul(general_use_authentication, NULL, 10)) == 1) &&
				   ((find_arg(data, "general_authentication_secret", general_authentication_secret,
				              CONFIGURATION_SECRET_MAX_LENGTH + 1)) > 0)) { // +1 for NULL-terminator
					os_memcpy(configuration_current_to_save.general_authentication_secret,
					          general_authentication_secret, CONFIGURATION_SECRET_MAX_LENGTH);
				}
				else{
					os_memset(configuration_current_to_save.general_authentication_secret, 0, CONFIGURATION_SECRET_MAX_LENGTH);
				}
			}
			else{
				os_memset(configuration_current_to_save.general_authentication_secret, 0, CONFIGURATION_SECRET_MAX_LENGTH);
			}

			// Client and AP mode settings based on operating mode.
			if((find_arg(data, "general_mode", general_mode, 2)) > 0) {
				unsigned long numeric_general_mode = strtoul(general_mode, NULL, 10);

				// Only client mode.
				if(numeric_general_mode == 0) {
					configuration_current_to_save.ap_enable = 0;
					configuration_current_to_save.client_enable = 1;

					do_update_settings_client(data);
				}

				// Only AP mode.
				else if(numeric_general_mode == 1) {
					configuration_current_to_save.ap_enable = 1;
					configuration_current_to_save.client_enable = 0;

					do_update_settings_ap(data);
				}

				// Client and AP mode.
				else if(numeric_general_mode == 2) {
					configuration_current_to_save.ap_enable = 1;
					configuration_current_to_save.client_enable = 1;

					do_update_settings_ap(data);
					do_update_settings_client(data);
				}
			}

			// Update configuration in EEPROM
			configuration_save_to_eeprom();
			// Reset stack.
			tfp_handle_packet(tf_reset_packet, 8);
		}
	}

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_authenticate_html(HttpdConnData *connection_data) {
	if((do_check_session(connection_data)) == 1) {
		connection_data->cgiArg = "/index.html";

		return cgiRedirect(connection_data);
	}

	connection_data->url = "/authenticate.html";

	return cgiEspFsHook(connection_data);
}

int ICACHE_FLASH_ATTR cgi_is_already_authneticated(HttpdConnData *connection_data) {
	char response[GENERIC_BUFFER_SIZE];

	char cookie[GENERIC_BUFFER_SIZE];

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(connection_data->post->buff,
					 	 JSON_REQUEST_IS_ALREADY_AUTHENTICATED)) == 1)) {
		sprintf(response,
				fmt_response_json,
				JSON_REQUEST_IS_ALREADY_AUTHENTICATED,
				JSON_STATUS_OK,
				"null");
	}
	else {
		sprintf(response,
				fmt_response_json,
				JSON_REQUEST_IS_ALREADY_AUTHENTICATED,
				JSON_STATUS_FAILED,
				"null");
	}

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR do_initialize_authentication_slots(void) {
	for(int i = 0; i < MAX_AUTHENTICATION_SLOTS; i++) {
		authentication_slots[i] = 0;
	}

	return 1;
}

int ICACHE_FLASH_ATTR do_check_secret(uint8_t slot, uint8_t *bytes_rn_client,
                                      uint8_t *bytes_sha1_hmac_client){
		uint8_t data[8];
		uint8_t bytes_sha1_hmac_server[20];

		data[0] = (authentication_slots[slot] >> 24) & 0xFF;
		data[1] = (authentication_slots[slot] >> 16) & 0xFF;
		data[2] = (authentication_slots[slot] >> 8) & 0xFF;
		data[3] = authentication_slots[slot] & 0xFF;

		os_memcpy(&data[4], bytes_rn_client, 4);

		if(hmac_sha1(configuration_current_to_save.general_authentication_secret,
		             strnlen(configuration_current_to_save.general_authentication_secret, CONFIGURATION_SECRET_MAX_LENGTH),
		             data, 8, bytes_sha1_hmac_server) != 0){
			return -1;
		}

		for(uint8_t i = 0; i < 20; i++){
			if(bytes_sha1_hmac_server[i] != bytes_sha1_hmac_client[i]){
				return -1;
			}
		}

		return 1;
}

int ICACHE_FLASH_ATTR cgi_init_authentication(HttpdConnData *connection_data){
	uint8_t byte_array_rn_server[4];
	char response[GENERIC_BUFFER_SIZE];
	char response_data[GENERIC_BUFFER_SIZE];
	char *fmt_response_data_json = "{\"slot\":%d,\"bytes_rn_server\":[%d, %d, %d, %d]}";

	if(do_check_request(connection_data->post->buff, JSON_REQUEST_INIT_AUTHENTICATION) == 1){
		do{
			authentication_slots[current_authentication_slot_index] = os_random();
		}
		while(authentication_slots[current_authentication_slot_index] <= 0);

		byte_array_rn_server[0] =
		(authentication_slots[current_authentication_slot_index] >> 24) & 0xFF;

		byte_array_rn_server[1] =
		(authentication_slots[current_authentication_slot_index] >> 16) & 0xFF;

		byte_array_rn_server[2] =
		(authentication_slots[current_authentication_slot_index] >> 8) & 0xFF;

		byte_array_rn_server[3] =
		authentication_slots[current_authentication_slot_index] & 0xFF;

		sprintf(response_data,
			fmt_response_data_json,
			current_authentication_slot_index,
			byte_array_rn_server[0],
			byte_array_rn_server[1],
			byte_array_rn_server[2],
			byte_array_rn_server[3]);

		sprintf(response,
			fmt_response_json,
			JSON_REQUEST_INIT_AUTHENTICATION,
			JSON_STATUS_OK,
			response_data);

		if(current_authentication_slot_index == MAX_AUTHENTICATION_SLOTS - 1){
			current_authentication_slot_index = 0;
		}
		else{
			current_authentication_slot_index++;
		}
	}
	else{
		sprintf(response,
			fmt_response_json,
			JSON_REQUEST_INIT_AUTHENTICATION,
			JSON_STATUS_FAILED,
			"null");
	}

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}
