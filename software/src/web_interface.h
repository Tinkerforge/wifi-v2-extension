#include "httpd.h"
#include "c_types.h"

// Defines.
#define JSON_REQUEST_AUTHENTICATE 1
#define JSON_REQUEST_GET_STATUS 2
#define JSON_REQUEST_UPDATE_SETTINGS 3
#define JSON_REQUEST_END_SESSION 4
#define JSON_REQUEST_IS_ALREADY_AUTHENTICATED 5
#define JSON_REQUEST_GET_SETTINGS 6

#define JSON_STATUS_OK 1
#define JSON_STATUS_FAILED 2

#define HEADER_FIELD_TYPE_STRING 1
#define HEADER_FIELD_TYPE_NUMERIC 2
#define GENERIC_BUFFER_SIZE 1024
#define MAX_ACTIVE_SESSION_COOKIES 32

// Global variables.
extern unsigned long active_sessions[MAX_ACTIVE_SESSION_COOKIES];

// Structs.
struct get_status {
	uint8 operating_mode;
	char client_status[15];
	uint8 signal_strength;
	char client_ip[16];
	char client_netmask[16];
	char client_gateway[16];
	char client_mac[18];
	unsigned long client_rx_count;
	unsigned long client_tx_count;
	uint8 ap_connected_clients;
	char ap_ip[16];
	char ap_netmask[16];
	char ap_gateway[16];
	char ap_mac[18];
	unsigned long ap_rx_count;
	unsigned long ap_tx_count;
};

// Function prototypes.
int ICACHE_FLASH_ATTR do_is_auth_enabled(void);
int ICACHE_FLASH_ATTR do_get_sid(unsigned long *sid);
int ICACHE_FLASH_ATTR do_get_cookie_field(char *cookie,
										  char *field,
										  unsigned char type_field,
										  void *value);
int ICACHE_FLASH_ATTR do_has_cookie(HttpdConnData *connection_data,
								 	char *cookie,
								  	unsigned long length);
int ICACHE_FLASH_ATTR do_check_session(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_404(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_get_status(struct get_status *status,
									unsigned char init_only);
int ICACHE_FLASH_ATTR cgi_root(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_ping_pong(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_check_request(char *buffer_post, uint8 rid);
int ICACHE_FLASH_ATTR cgi_get_status(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_end_session(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_authenticate(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_get_settings(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_initialize_web_interface_session_tracking(void);
int ICACHE_FLASH_ATTR cgi_update_settings(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_authenticate_html(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_is_already_authneticated(HttpdConnData *connection_data);
