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
#include "debug.h"
#include "i2c_master.h"
#include "eeprom.h"
#include "logging.h"
#include "configuration.h"
extern Configuration configuration_current;

void ICACHE_FLASH_ATTR user_init_done_cb(void) {
	configuration_apply_post_init();

	uart_con_init();

	// TFP connection is opened regardless of whether mesh is enabled or not.
	tfp_open_connection();

	#if(MESH_ENABLED != 1)
		tfpw_open_connection();

		if(configuration_current.general_website_port > 1) {
			http_open_connection();
		}
	#endif
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

	// Initially configure GPIO2 as UART1 TX.
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);

	i2c_master_init();
	eeprom_init();
	configuration_load_from_eeprom();

	// The documentation says we can not call station_connect and similar
	// in user_init, so we do it in the callback after it is done!
	configuration_apply_during_init();
	system_init_done_cb(user_init_done_cb);
}
