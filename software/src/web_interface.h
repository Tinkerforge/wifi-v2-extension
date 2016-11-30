/* WIFI Extension 2.0
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * web_interface.h: Web interface for WIFI Extension
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

 #ifndef WEB_INTERFACE_H
 #define WEB_INTERFACE_H

#include "httpd.h"
#include "c_types.h"

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
int ICACHE_FLASH_ATTR do_update_settings_ap(char *data);
int ICACHE_FLASH_ATTR do_has_cookie(HttpdConnData *connection_data,
	char *cookie,
	unsigned long length);
int ICACHE_FLASH_ATTR do_update_settings_client(char *data);
int ICACHE_FLASH_ATTR do_get_status(struct get_status *status,
	unsigned char init_only);
int ICACHE_FLASH_ATTR cgi_404(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_root(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_check_request(char *buffer_post, uint8 rid);
int ICACHE_FLASH_ATTR cgi_get_status(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_end_session(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_check_session(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_authenticate(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_get_settings(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_initialize_web_interface_session_tracking(void);
int ICACHE_FLASH_ATTR cgi_update_settings(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_authenticate_html(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR cgi_is_already_authneticated(HttpdConnData *connection_data);
int ICACHE_FLASH_ATTR do_check_secret(uint8_t slot, uint8_t *bytes_rn_client,
	uint8_t *bytes_sha1_hmac_client);
int ICACHE_FLASH_ATTR do_initialize_authentication_slots(void);
int ICACHE_FLASH_ATTR cgi_init_authentication(HttpdConnData *connection_data);

#endif
