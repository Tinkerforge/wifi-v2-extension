#include "espmissingincludes.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "drivers/uart.h"
#include "tfp_connection.h"
#include "uart_connection.h"
#include "config.h"
#include "debug.h"

void wifi_handle_event_cb(System_Event_t *evt) {
	os_printf("event %x\n", evt->event);
}

//Init function 
void ICACHE_FLASH_ATTR user_init() {
	debug_enable(UART_DEBUG);
	uart_con_init();
    os_printf("user_init\n");

	const char ssid[] = "Tinkerforge WLAN";
	const char password[] = "25842149320894763607";

	struct station_config conf;

	wifi_set_event_handler_cb(wifi_handle_event_cb);
	wifi_set_sleep_type(NONE_SLEEP_T);
	wifi_set_opmode(STATION_MODE);
	os_memcpy(&conf.ssid, ssid, sizeof(ssid));
	os_memcpy(&conf.password, password, sizeof(password));
	conf.bssid_set = 0;
	wifi_station_set_config(&conf);
	wifi_station_connect();

	os_printf("open_tfp_connection\n");
	tfp_open_connection();


/*
    // Initialize the GPIO subsystem.
    gpio_init();

    //Set GPIO4 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);

    //Set GPIO4 low
    gpio_output_set(0, BIT4, BIT4, 0);
        //Do blinky stuff
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT4) {
        //Set GPIO4 to LOW
        gpio_output_set(0, BIT4, BIT4, 0);
    } else {
        //Set GPIO4 to HIGH
        gpio_output_set(BIT4, 0, BIT4, 0);
    }

    */
    

}
