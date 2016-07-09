#include <string.h>
#include <esp8266.h>
#include "httpdespfs.h"
#include "web_interface.h"
#include "configuration.h"
#include "communication.h"

extern GetWifi2StatusReturn gw2sr;

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

const char *fmt_set_session_cookie = "sid=%lu; expires=0; path=/";

extern Configuration configuration_current;
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
	if ((httpdGetHeader(connection_data,
						"Cookie",
						cookie,
						length)) == 1)
		return 1;
	else 
		return -1;
}

int ICACHE_FLASH_ATTR do_check_session(HttpdConnData *connection_data) {
	unsigned long sid;
	char cookie[GENERIC_BUFFER_SIZE];

	if((do_is_auth_enabled()) == -1) {
		return 1;
	}

	if ((do_has_cookie(connection_data,
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

int ICACHE_FLASH_ATTR cgi_get_status(HttpdConnData *connection_data) {
	struct get_status status;
	char response[GENERIC_BUFFER_SIZE];
	char response_status_data[GENERIC_BUFFER_SIZE];

	httpdStartResponse(connection_data, 200);
	
	if((do_check_session(connection_data)) == 1) {
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
	char request[8];
	char cookie[GENERIC_BUFFER_SIZE];
	char response[GENERIC_BUFFER_SIZE];

	httpdStartResponse(connection_data, 200);
	httpdEndHeaders(connection_data);

	if ((do_has_cookie(connection_data,
					   cookie,
					   GENERIC_BUFFER_SIZE)) == 1) {
		if ((httpdFindArg(connection_data->post->buff,
				 	  	  "request",
				 	  	  request,
				 	  	  8)) > 0) {
			if(atoi(request) == JSON_REQUEST_END_SESSION) {
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
	char response[GENERIC_BUFFER_SIZE];
	char header_set_cookie[GENERIC_BUFFER_SIZE];
	char secret[CONFIGURATION_SECRET_MAX_LENGTH];

	httpdStartResponse(connection_data, 200);

	if ((httpdFindArg(connection_data->post->buff,
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

	httpdEndHeaders(connection_data);

	sprintf(response,
			fmt_response_json,
			JSON_REQUEST_AUTHENTICATE,
			JSON_STATUS_FAILED,
			"null");

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

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_do_authenticate_html(HttpdConnData *connection_data) {
	if((do_check_session(connection_data)) == 1) {
		connection_data->cgiArg = "/index.html";
		
		return cgiRedirect(connection_data);
	}

	connection_data->url = "/authenticate.html";
	
	return cgiEspFsHook(connection_data);
}
