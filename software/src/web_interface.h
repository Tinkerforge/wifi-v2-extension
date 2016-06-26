#include "httpd.h"
#include "c_types.h"

#define MAX_WEB_INTERFACE_SESSIONS 50

extern unsigned long active_sessions[MAX_WEB_INTERFACE_SESSIONS];

int ICACHE_FLASH_ATTR cgiDo404(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiDoRoot(HttpdConnData *connection_data);
void ICACHE_FLASH_ATTR initializeWebInterfaceSessionTracking(void);
int ICACHE_FLASH_ATTR cgiGetStatus(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiEndSession(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR doAuthentication(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiUpdateSettings(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgiDoAuthentication(HttpdConnData *connection_data);
