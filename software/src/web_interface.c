#include <string.h>
#include <esp8266.h>
#include "httpdespfs.h"
#include "web_interface.h"
#include "configuration.h"
#include "communication.h"
#include "tfp_connection.h"

extern GetWifi2StatusReturn gw2sr;
extern Configuration configuration_current;

const char *fmt_response_json = \
	"{\"request\":%d,\"status\":%d,\"data\":%s}";

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
\"client_password\":\"%s\", \
\"ap_enable\":%d, \
\"ap_ssid\":\"%s\", \
\"ap_ip\":[%d,%d,%d,%d], \
\"ap_subnet_mask\":[%d,%d,%d,%d], \
\"ap_gateway\":[%d,%d,%d,%d], \
\"ap_encryption\":%d, \
\"ap_hidden\":%d, \
\"ap_mac_address\":[%d,%d,%d,%d,%d,%d], \
\"ap_channel\":%d, \
\"ap_hostname\":\"%s\", \
\"ap_password\":\"%s\"\
}";

const char *fmt_set_session_cookie = "sid=%lu";

unsigned long active_sessions[MAX_ACTIVE_SESSION_COOKIES];

int ICACHE_FLASH_ATTR do_get_sid(unsigned long *sid) {
	for(int i = 0; i < MAX_ACTIVE_SESSION_COOKIES; i++) {
		if(active_sessions[i] == 0) {
			do {
				*sid = os_random();
			}
			while(*sid <= 0);

			active_sessions[i] = *sid;

			return 1;
		}
	}

	return -1;
}

int ICACHE_FLASH_ATTR do_is_auth_enabled(void) {
	if((strcmp(configuration_current.general_authentication_secret, "")) == 0)
		return -1;

	return 1;
}

int ICACHE_FLASH_ATTR do_get_cookie_field(char *cookie,
										  char *field,
										  unsigned char type_field,
										  void *value) {
	char *token;
	char _cookie[GENERIC_BUFFER_SIZE];

	if(!strstr(cookie, field)) {
		return -1;
	}

	strcpy(_cookie, cookie);

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

int ICACHE_FLASH_ATTR cgi_404(HttpdConnData *connection_data) {
	connection_data->url = "/not_found.html";

	return cgiEspFsHook(connection_data);
}

int ICACHE_FLASH_ATTR do_check_request(char *buffer_post, uint8 rid) {
	char request[4];

	if ((httpdFindArg(buffer_post, "request", request, 4)) > 0) {
		if(atoi(request) == rid) {
			return 1;
		}
	}

	return 0;
}

int ICACHE_FLASH_ATTR cgi_get_status(HttpdConnData *connection_data) {
	struct get_status status;
	char response[GENERIC_BUFFER_SIZE];
	char response_status_data[GENERIC_BUFFER_SIZE];

	httpdStartResponse(connection_data, 200);

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

	httpdEndHeaders(connection_data);
	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_root(HttpdConnData *connection_data) {
	if((do_check_session(connection_data)) == 1) {
		connection_data->url = "/index.html";
		
		return cgiEspFsHook(connection_data);
	}

	connection_data->cgiArg = "/authenticate.html";

	return cgiRedirect(connection_data);
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
	httpdEndHeaders(connection_data);

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

							sprintf(response,
									fmt_response_json,
									JSON_REQUEST_GET_STATUS,
									JSON_STATUS_OK,
									"null");

							httpdSend(connection_data, response, -1);

							return HTTPD_CGI_DONE;
						}
					}
				}
			}
	}

	sprintf(response,
			fmt_response_json,
			JSON_REQUEST_GET_STATUS,
			JSON_STATUS_FAILED,
			"null");

	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_authenticate(HttpdConnData *connection_data) {
	unsigned long sid;
	char data[GENERIC_BUFFER_SIZE];
	char response[GENERIC_BUFFER_SIZE];
	char header_set_cookie[GENERIC_BUFFER_SIZE];
	char secret[CONFIGURATION_SECRET_MAX_LENGTH];

	httpdStartResponse(connection_data, 200);

	if(((do_check_request(connection_data->post->buff,
						 JSON_REQUEST_AUTHENTICATE)) == 1) &&
		((httpdFindArg(connection_data->post->buff,
				 		"data",
				 		data,
				 		GENERIC_BUFFER_SIZE)) > 0)) {
			if((httpdFindArg(data,
				 			 "secret",
				 			 secret,
				 			 CONFIGURATION_SECRET_MAX_LENGTH)) > 0) {
				if((strcmp(configuration_current.general_authentication_secret, secret)) == 0) {
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
	}

	httpdEndHeaders(connection_data);

	sprintf(response,
			fmt_response_json,
			JSON_REQUEST_AUTHENTICATE,
			JSON_STATUS_FAILED,
			"null");

	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_get_settings(HttpdConnData *connection_data) {
	char response[GENERIC_BUFFER_SIZE];
	char response_settings[GENERIC_BUFFER_SIZE];

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(connection_data->post->buff,
					 	  JSON_REQUEST_GET_SETTINGS)) == 1)) {
	 	sprintf(response_settings,
				fmt_response_json_settings_data,
				configuration_current.general_port,
				configuration_current.general_websocket_port,
				configuration_current.general_website_port,
				configuration_current.general_phy_mode,
				configuration_current.general_sleep_mode,
				configuration_current.general_website,
				configuration_current.general_authentication_secret,
				configuration_current.client_enable,
				configuration_current.client_ssid,
				configuration_current.client_ip[0],
				configuration_current.client_ip[1],
				configuration_current.client_ip[2],
				configuration_current.client_ip[3],
				configuration_current.client_subnet_mask[0],
				configuration_current.client_subnet_mask[1],
				configuration_current.client_subnet_mask[2],
				configuration_current.client_subnet_mask[3],
				configuration_current.client_gateway[0],
				configuration_current.client_gateway[1],
				configuration_current.client_gateway[2],
				configuration_current.client_gateway[3],
				configuration_current.client_mac_address[0],
				configuration_current.client_mac_address[1],
				configuration_current.client_mac_address[2],
				configuration_current.client_mac_address[3],
				configuration_current.client_mac_address[4],
				configuration_current.client_mac_address[5],
				configuration_current.client_bssid[0],
				configuration_current.client_bssid[1],
				configuration_current.client_bssid[2],
				configuration_current.client_bssid[3],
				configuration_current.client_bssid[4],
				configuration_current.client_bssid[5],
				configuration_current.client_hostname,
				configuration_current.client_password,
				configuration_current.ap_enable,
				configuration_current.ap_ssid,
				configuration_current.ap_ip[0],
				configuration_current.ap_ip[1],
				configuration_current.ap_ip[2],
				configuration_current.ap_ip[3],
				configuration_current.ap_subnet_mask[0],
				configuration_current.ap_subnet_mask[1],
				configuration_current.ap_subnet_mask[2],
				configuration_current.ap_subnet_mask[3],
				configuration_current.ap_gateway[0],
				configuration_current.ap_gateway[1],
				configuration_current.ap_gateway[2],
				configuration_current.ap_gateway[3],
				configuration_current.ap_encryption,
				configuration_current.ap_hidden,
				configuration_current.ap_mac_address[0],
				configuration_current.ap_mac_address[1],
				configuration_current.ap_mac_address[2],
				configuration_current.ap_mac_address[3],
				configuration_current.ap_mac_address[4],
				configuration_current.ap_mac_address[5],
				configuration_current.ap_channel,
				configuration_current.ap_hostname,
				configuration_current.ap_password);

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

	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR do_initialize_web_interface_session_tracking(void) {
	for(int i = 0; i < MAX_ACTIVE_SESSION_COOKIES; i++) {
		active_sessions[i] = 0;
	}

	return 1;
}

int ICACHE_FLASH_ATTR cgi_update_settings(HttpdConnData *connection_data) {
	const uint8_t tf_reset_packet[8] = {0, 0, 0, 0, 8, 243, 0, 0};

	char data[GENERIC_BUFFER_SIZE];

	char general_website_port[6];

	//httpdStartResponse(connection_data, 200);
	//httpdEndHeaders(connection_data);

	if(((do_check_session(connection_data)) == 1) &&
	   ((do_check_request(connection_data->post->buff,
					 	  JSON_REQUEST_UPDATE_SETTINGS)) == 1)) {
		if((httpdFindArg(connection_data->post->buff,
					 	 "data",
					 	 data,
					 	 GENERIC_BUFFER_SIZE)) > 0) {

			// Get all form fields from data.
			if((httpdFindArg(data, "general_website_port", general_website_port, 6)) > 0) {
				// Save settings.
				configuration_current.general_website_port = atoi(general_website_port);
				configuration_save_to_eeprom();

				// Reset stack.
				tfp_handle_packet(tf_reset_packet, 8);
			}
		}
	}
	else {
	}

	//httpdSend(connection_data, response, -1);

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

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);

	char cookie[GENERIC_BUFFER_SIZE];

	if((httpdGetHeader(connection_data,
					   "Cookie",
					   cookie,
					   GENERIC_BUFFER_SIZE)) == 1)

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

	httpdSend(connection_data, response, -1);

	return HTTPD_CGI_DONE;
}
