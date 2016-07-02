#include "httpd.h"
#include "c_types.h"

// Defines.
#define MAX_WEB_INTERFACE_SESSIONS 50

// Global variables.
extern unsigned long active_sessions[MAX_WEB_INTERFACE_SESSIONS];

// Structs.
struct getStatus {
	unsigned char client_enabled[4];
	unsigned char client_status;
	int client_signal_strength;
	unsigned char client_ip0;
	unsigned char client_ip1;
	unsigned char client_ip2;
	unsigned char client_ip3;
	unsigned char client_subnet_mask0;
	unsigned char client_subnet_mask1;
	unsigned char client_subnet_mask2;
	unsigned char client_subnet_mask3;
	unsigned char client_gateway0;
	unsigned char client_gateway1;
	unsigned char client_gateway2;
	unsigned char client_gateway3;
	unsigned char client_mac0;
	unsigned char client_mac1;
	unsigned char client_mac2;
	unsigned char client_mac3;
	unsigned char client_mac4;
	unsigned char client_mac5;
	int client_rx_count;
	int client_tx_count;
	char ap_enabled[4];
	int ap_connected_clients;
	unsigned char ap_ip0;
	unsigned char ap_ip1;
	unsigned char ap_ip2;
	unsigned char ap_ip3;
	unsigned char ap_subnet_mask0;
	unsigned char ap_subnet_mask1;
	unsigned char ap_subnet_mask2;
	unsigned char ap_subnet_mask3;
	unsigned char ap_gateway0;
	unsigned char ap_gateway1;
	unsigned char ap_gateway2;
	unsigned char ap_gateway3;
	unsigned char ap_mac0;
	unsigned char ap_mac1;
	unsigned char ap_mac2;
	unsigned char ap_mac3;
	unsigned char ap_mac4;
	unsigned char ap_mac5;
	int ap_rx_count;
	int ap_tx_count;
};

// Function prototypes.
int ICACHE_FLASH_ATTR doGetStatus(struct getStatus *status);
int ICACHE_FLASH_ATTR cgi404(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiRoot(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiGetStatus(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiEndSession(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR doInitializeWebInterfaceSessionTracking(void);
int ICACHE_FLASH_ATTR cgiAuthenticate(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR doAuthentication(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiUpdateSettings(HttpdConnData *connection_data);
