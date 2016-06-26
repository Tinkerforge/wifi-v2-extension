#include <esp8266.h>
#include "httpdespfs.h"
#include "web_interface.h"
#include "configuration.h"

extern Configuration configuration_current;
unsigned long active_sessions[MAX_WEB_INTERFACE_SESSIONS];

/*
os_printf("\n***  POST DATA = %s ***\n", connection_data->post->buff);
os_printf("\n***  POST DATA LEN = %d ***\n", connection_data->post->len);
*/

int ICACHE_FLASH_ATTR cgiDo404(HttpdConnData *connection_data) {
	connection_data->url = "/not_found.html";

	return cgiEspFsHook(connection_data);
}

int ICACHE_FLASH_ATTR cgiDoRoot(HttpdConnData *connection_data) {
	if (doAuthentication(connection_data)) {
		connection_data->url = "/index.html";

		return cgiEspFsHook(connection_data);
	}
	else {
		connection_data->url = "/authenticate.html";

		return cgiEspFsHook(connection_data);
	}
}

void ICACHE_FLASH_ATTR initializeWebInterfaceSessionTracking(void) {
	for(int i = 0; i < MAX_WEB_INTERFACE_SESSIONS; i++) {
		active_sessions[i] = (unsigned long)0;
	}
}

int ICACHE_FLASH_ATTR cgiGetStatus(HttpdConnData *connection_data) {
	/*if (doAuthentication()) {

	}
	else {
		httpdRedirect(connection_data, "/authenticate.html");
	}*/

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiEndSession(HttpdConnData *connection_data) {
	/*if (doAuthentication()) {

	}
	else {
		httpdRedirect(connection_data, "/authenticate.html");
	}*/

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR doAuthentication(HttpdConnData *connection_data) {
	if (strcmp(configuration_current.general_authentication_secret, "") == 0) {
		os_printf("\n** AUTH DISABLED **\n");
		return 1;
	}
	else {
		os_printf("\n** AUTH ENABLED **\n");
		//return 0;
		return 1;
	}
}

int ICACHE_FLASH_ATTR cgiUpdateSettings(HttpdConnData *connection_data) {
	/*if (doAuthentication()) {

	}
	else {
		httpdRedirect(connection_data, "/authenticate.html");
	}*/

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiDoAuthentication(HttpdConnData *connection_data) {
	return 0;
}
