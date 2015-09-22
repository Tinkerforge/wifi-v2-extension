/* ESP8266 drivers
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * uart.c: Implementation of uart access
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

#include "uart.h"

#include "espmissingincludes.h"
#include "user_interface.h"
#include "c_types.h"
#include "osapi.h"
#include "os_type.h"

static int8_t uart_printf = -1;

void ICACHE_FLASH_ATTR uart_configure(uint8_t uart, uint32_t baudrate, UartParity parity, UartStopBits stop_bits, UartDataBits data_bits) {
    uart_div_modify(uart, UART_CLK_FREQ / baudrate);

    uint8_t enable_parity = 0;
    if(parity != UART_PARITY_NONE) {
    	enable_parity = 1;
    }

    WRITE_PERI_REG(UART_CONF0(uart),   ((enable_parity & UART_PARITY_EN_M) << UART_PARITY_EN_S)
                                     | ((parity & UART_PARITY_M)  <<UART_PARITY_S)
                                     | ((stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                                     | ((data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));

    // Clear FIFOs
    SET_PERI_REG_MASK(UART_CONF0(uart), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart), UART_RXFIFO_RST | UART_TXFIFO_RST);
}

void ICACHE_FLASH_ATTR uart_enable(uint8_t uart) {
    if(uart == UART1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    } else {
#if 0
    	// If swapped
#else
    	// If unswapped
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
#endif

    }
}

void ICACHE_FLASH_ATTR uart_swap_uart0(void) {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0RTS/*FUNC_U0CTS*/);//CONFIG MTCK PIN FUNC TO U0CTS
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);//CONFIG MTDO PIN FUNC TO U0RTS
	SET_PERI_REG_MASK(0x3ff00028 , BIT2);//SWAP PIN : U0TXD<==>U0RTS(MTDO) , U0RXD<==>U0CTS(MTCK)*/

   	//system_uart_swap();
}

uint8_t ICACHE_FLASH_ATTR uart_get_rx_fifo_count(uint8_t uart) {
	return (READ_PERI_REG(UART_STATUS(uart)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;
}

uint8_t ICACHE_FLASH_ATTR uart_get_tx_fifo_count(uint8_t uart) {
	return (READ_PERI_REG(UART_STATUS(uart)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT;
}

uint8_t ICACHE_FLASH_ATTR uart_rx(uint8_t uart) {
	return READ_PERI_REG(UART_FIFO(uart)) & 0xFF;
}

void ICACHE_FLASH_ATTR uart_tx(uint8_t uart, uint8_t c) {
	while(uart_get_tx_fifo_count(uart) >= 126) {
		os_printf("uart_get_tx_fifo_count(uart) >= 126");
	}
    WRITE_PERI_REG(UART_FIFO(uart), c);
}

void ICACHE_FLASH_ATTR uart_printf_write_char(char c) {
    if(c == '\n') {
        uart_tx(uart_printf, '\r');
        uart_tx(uart_printf, '\n');
    } else if(c == '\r') {

    } else {
    	uart_tx(uart_printf, c);
    }
}

void ICACHE_FLASH_ATTR uart_set_printf(uint8_t uart) {
	uart_printf = uart;
	os_install_putc1((void *)uart_printf_write_char);
}
