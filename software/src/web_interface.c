//#include "http_connection.h"

//#define HTTP_ESPFS_POS       0x00100000

#include <esp8266.h>

#include "configuration.h"

#include "platform.h"
//#include "httpd.h"
#include "httpdespfs.h"
//#include "espfs.h"
//#include "logging.h"
#include "web_interface.h"

int ICACHE_FLASH_ATTR myTest(HttpdConnData *connData)
{
	httpdRedirect(connData, "/index.html");

	return HTTPD_CGI_DONE;
}
