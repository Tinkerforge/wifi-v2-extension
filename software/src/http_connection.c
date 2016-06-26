/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * http_connection.c: Webserver for WIFI Extension
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

#include "http_connection.h"

#define HTTP_ESPFS_POS       0x00100000

#include "configuration.h"

#include "platform.h"
#include "httpd.h"
#include "httpdespfs.h"
#include "espfs.h"
#include "logging.h"
#include "web_interface.h"

extern Configuration configuration_current;

HttpdBuiltInUrl builtInUrls[] = {
	{"/", cgiDoRoot, NULL},
	{"/js/*", cgiEspFsHook, NULL},
	{"/css/*", cgiEspFsHook, NULL},
	{"/fonts/*", cgiEspFsHook, NULL},
	{"/resources/*", cgiEspFsHook, NULL},
	{"/index.html", cgiDoRoot, NULL},
	{"/authenticate.html", cgiEspFsHook, "/authenticate.html"},
	{"/get_status.cgi", cgiGetStatus, NULL},
	{"/end_session.cgi", cgiEndSession, NULL},
	{"/authenticate.cgi", cgiDoAuthentication, NULL},
	{"/update_settings.cgi", cgiUpdateSettings, NULL},
	{"*", cgiDo404, NULL},
	{NULL, NULL, NULL}
};

void ICACHE_FLASH_ATTR http_open_connection(void) {
	espFsInit((void*)(HTTP_ESPFS_POS));
	initializeWebInterfaceSessionTracking();
	httpdInit(builtInUrls, configuration_current.general_website_port);
}
