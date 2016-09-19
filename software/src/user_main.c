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

extern Configuration configuration_current;

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

	if(configuration_current.general_website_port > 1) {
		http_open_connection();
	}
}
