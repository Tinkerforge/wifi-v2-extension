/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * user_main.c: Handling of communication between PC and WIFI Extension
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

#include "espmissingincludes.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "drivers/uart.h"
#include "tfp_connection.h"
#include "http_connection.h"
#include "tfp_websocket_connection.h"
#include "uart_connection.h"
#include "config.h"
#include "debug.h"
#include "i2c_master.h"
#include "eeprom.h"
#include "logging.h"
#include "configuration.h"

// Mesh interface defines.
#include "mesh.h"

/*
 * Ultimately this define will be replaced with a configuration a field to
 * determine whether the extension is in mesh mode or not. Actual application of
 * mesh settings will be implemented in the function configuration_apply().
 * By default mesh mode is disabled.
 */
#define MESH_ENABLED 0

/*
 * Mesh related constants. Most of these constants will be the new configuration
 * fields for the mesh feature.
 */
#define MESH_MAX_HOP (4)
#define MESH_ROUTER_SSID_LEN 16
#define MESH_ROUTER_PASSWORD_LEN 20
#define MESH_AP_PASSWORD "1234567890"
#define MESH_AP_SSID_PREFIX "TF_MESH_DEV"
#define MESH_ROUTER_SSID "Tinkerforge WLAN"
#define MESH_AP_AUTHENTICATION AUTH_WPA2_PSK
#define MESH_ROUTER_PASSWORD "25842149320894763607"

static const uint16_t MESH_SERVICE_PORT = 7000;
static const uint8_t MESH_SERVICE_IP[4] = {192, 168, 178, 67};
static const uint8_t MESH_GROUP_ID[6] = {0x18, 0xFE, 0x34, 0x00, 0x00, 0x50};
static const uint8_t MESH_ROUTER_BSSID[6] = {0xF0, 0xB4, 0x29, 0x2C, 0x7C, 0x72};

extern Configuration configuration_current;

// Mesh enable callback.
void ICACHE_FLASH_ATTR cb_enable_mesh() {
	uint8_t count_p;
	uint8_t count_c;
	uint8_t *inf[128];
	struct station_config router;

	espconn_mesh_get_router(&router);
	espconn_mesh_get_node_info(MESH_NODE_PARENT, inf, &count_p);
	espconn_mesh_get_node_info(MESH_NODE_CHILD, inf, &count_c);

	/*
	 * The following two functions seems to dependent on particular mesh library
	 * version.
	 */
	//espconn_mesh_is_root();
	//espconn_mesh_is_root_candidate();

	os_printf("MSH:CB_ENB\n");
	espconn_mesh_print_ver();
	os_printf("MSH:PARENT=%d\n", count_p);
	os_printf("MSH:CHILD=%d\n", count_c);
	os_printf("MSH:ROUTER=%s\n", router.ssid);
	os_printf("MSH:STATUS=%d\n", espconn_mesh_get_status());
}

void ICACHE_FLASH_ATTR user_init() {
	#ifdef DEBUG_ENABLED
		debug_enable(UART_DEBUG);
	#else
		system_set_os_print(0);
	#endif

	logd("user_init\n");
	gpio_init();
	wifi_status_led_install(12, PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);

	#ifdef DEBUG_ENABLED
		configuration_use_default();
	#else
		i2c_master_init();
		eeprom_init();
		configuration_load_from_eeprom();
	#endif

	configuration_apply();

	uart_con_init();
	tfp_open_connection();
	tfpw_open_connection();

	// Temporary mesh settings application for experimentation.
	#if (MESH_ENABLED == 1)
		struct station_config config_mesh_router;

		// Router configuration.
		os_memset(&config_mesh_router, 0, sizeof(config_mesh_router));
		os_memcpy(config_mesh_router.ssid, MESH_ROUTER_SSID, MESH_ROUTER_SSID_LEN);
		os_memcpy(config_mesh_router.password, MESH_ROUTER_PASSWORD,
			MESH_ROUTER_PASSWORD_LEN);

	 /*
		* The following is required if the router AP is hidden. In which case the
		* BSSID or the MAC address of the router AP must be specified.
		*/
		config_mesh_router.bssid_set = 1;
		os_memcpy(config_mesh_router.bssid, MESH_ROUTER_BSSID,
			sizeof(config_mesh_router.bssid));

		/*
		 * So far it seems that the mesh nodes can be configured either to do a contest
		 * for choosing one root node or a particular node can be manually specified
		 * to be the root node of a particular mesh network. In both cases the root
		 * node needs to connect to an outgoing router AP. In this case the mesh will
		 * be an online mesh, offline mesh otherwise.
		 */
		if (!espconn_mesh_set_router(&config_mesh_router)) {
			os_printf("MSH_ERR:SETRTR\n");
		}

		// Node config.

		// MESH_GROUP_ID and MESH_SSID_PREFIX represent a mesh network.
		if (!espconn_mesh_group_id_init((uint8_t *)MESH_GROUP_ID,
		sizeof(MESH_GROUP_ID)/sizeof(MESH_GROUP_ID[0])) {
			os_printf("MSH_ERR:SETGRP\n");
		}

		if (!espconn_mesh_set_ssid_prefix(MESH_AP_SSID_PREFIX,
			sizeof(MESH_AP_SSID_PREFIX)/sizeof(MESH_AP_SSID_PREFIX[0])) {
			os_printf("MSH_ERR:SETPRF\n");
		}

		// Intra mesh node AP authentication.
		if (!espconn_mesh_encrypt_init(MESH_AP_AUTHENTICATION, MESH_AP_PASSWORD,
			sizeof(MESH_AP_PASSWORD)/sizeof(MESH_AP_PASSWORD[0])) {
			os_printf("MSH_ERR:SETAUT\n");
		}

		/*
		 * Maximum hops of the mesh network can be configured but consider heap space
		 * requirement according to the equation,
		 * (4^MESH_MAX_HOP - 1)/3 * 6.
		 */
		if (!espconn_mesh_set_max_hops(MESH_MAX_HOP)) {
			os_printf("MSH_ERR:MAXHOP=%d\n", espconn_mesh_get_max_hops());
		}

		// Mesh service socket. Basically for testing in this context.
		if (!espconn_mesh_server_init((struct ip_addr *)MESH_SERVICE_IP,
		MESH_SERVICE_PORT)) {
			os_printf("MSG_ERR:SRVINI\n");
		}

		// Enable mesh

		/*
		 * One post in the developer's forum mentioned for root node mesh must be
		 * enabled as MESH_LOCAL. MESH_ONLINE for non-root nodes. But couldn't find
		 * more information on this yet.
		 */
		espconn_mesh_enable(cb_enable_mesh, MESH_LOCAL);
	#endif

	if(configuration_current.general_website_port > 1) {
		http_open_connection();
	}
}
