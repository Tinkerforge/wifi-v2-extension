#include <esp8266.h>
#include "httpdespfs.h"
#include "web_interface.h"
#include "configuration.h"
#include "json/jsontree.h"
#include "json/jsonparse.h"

extern Configuration configuration_current;
unsigned long active_sessions[MAX_WEB_INTERFACE_SESSIONS];

int ICACHE_FLASH_ATTR doGetStatus(struct getStatus *status) {
	return 0;
}

int ICACHE_FLASH_ATTR cgi404(HttpdConnData *connection_data) {
	connection_data->url = "/not_found.html";

	return cgiEspFsHook(connection_data);
}

int ICACHE_FLASH_ATTR cgiRoot(HttpdConnData *connection_data) {
	if (doAuthentication(connection_data)) {
		connection_data->url = "/index.html";

		return cgiEspFsHook(connection_data);
	}
	else {
		connection_data->url = "/authenticate.html";

		return cgiEspFsHook(connection_data);
	}
}

int ICACHE_FLASH_ATTR doInitializeWebInterfaceSessionTracking(void) {
	for(int i = 0; i < MAX_WEB_INTERFACE_SESSIONS; i++) {
		active_sessions[i] = (unsigned long)0;
	}
	return 0;
}

int ICACHE_FLASH_ATTR cgiGetStatus(HttpdConnData *connection_data) {
	int l1,l2;
	char buf_sid[64];
	char buf_rid[64];
	char buf_data[64];
	char cookie_ua[64];
	char response_json[64];

	response_json[0] = '\0';
	httpdStartResponse(connection_data, 200);
	
	if ((httpdGetHeader(connection_data,
						"Cookie",
						cookie_ua,
						sizeof(response_json)) == 1)) {
		//printf("\n*** UA SENT COOKIE = %s ***\n",
		//	   cookie_ua);
	}
	else {
		//printf("\n*** SETTING COOKIE ***\n");
		httpdHeader(connection_data,
					"Set-Cookie",
					"SID=4223; Path=/;");
	}
httpdEndHeaders(connection_data);
	l1 = httpdFindArg(connection_data->post->buff,
				 "SID",
			  	 buf_sid,
				 sizeof(buf_sid));

	httpdFindArg(connection_data->post->buff,
				 "RID",
				 buf_rid,
				 sizeof(buf_rid));

	l2 = httpdFindArg(connection_data->post->buff,
				 "DATA",
				 buf_data,
				 sizeof(buf_data));

	//printf("\n*** UA_SID = %s ***\n", buf_sid);
	//printf("\n*** UA_RID = %s ***\n", buf_rid);
	//printf("\n*** UA_DATA = %s ***\n", buf_data);
	
	/*
	if (len == -1)
		printf("*** cgiGetStatus(), ERROR GETTING KEY: name ***\n");
	else
		printf("*** cgiGetStatus(), name = %s ***\n",
				  bufname);
	*/

	sprintf(response_json, "{\"%s\":\"%s\"}", buf_sid, buf_data);

	printf("*** cgiGetStatus(), RESPONSE = %s ***\n", response_json);


	httpdSend(connection_data, response_json, -1);
	//httpdSend(connection_data, "{\"key1\":\"strvalue\",\"key2\":23}", -1);

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
		printf("\n** AUTH DISABLED **\n");
		return 1;
	}
	else {
		printf("\n** AUTH ENABLED **\n");
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

int ICACHE_FLASH_ATTR cgiAuthenticate(HttpdConnData *connection_data) {
	return 0;
}
