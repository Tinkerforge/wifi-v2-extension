/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * uart_connection.c: Implementation of UART protocol between Master and Extension
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

#include "uart_connection.h"
#include "espmissingincludes.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "drivers/uart.h"
#include "config.h"
#include "tfp_connection.h"
#include "pearson_hash.h"
#include "tfp_connection.h"
#include "logging.h"

#define ACK_TIMEOUT_MS 10

#define uart_con_prio         0
#define uart_con_queue_length 1
static os_event_t uart_con_task[uart_con_queue_length];

#define UART_CON_HEADER_SIZE 3
#define UART_CON_BUFFER_SIZE (80 + UART_CON_HEADER_SIZE)
static uint8_t uart_con_buffer_recv[UART_CON_BUFFER_SIZE];
static uint8_t uart_con_buffer_recv_index = 0;
static uint8_t uart_con_buffer_recv_expected_length = 3;
static bool    uart_con_buffer_recv_wait_for_tfp = false;

static uint8_t uart_con_buffer_send[UART_CON_BUFFER_SIZE];
static uint8_t uart_con_buffer_send_size = 0;

static uint8_t uart_con_sequence_number = 1;
static uint8_t uart_con_last_sequence_number_seen = 0;

static bool wait_for_ack = false;

#define UART_CON_INDEX_LENGTH        0
#define UART_CON_INDEX_SEQUENCE      1
#define UART_CON_INDEX_TFP_START     2
#define UART_CON_INDEX_CHECKSUM(len) (len - 1)

static os_timer_t timeout_timer;

static void uart_con_send_next_try(void);

static void ICACHE_FLASH_ATTR ack_last_message(void) {
	const uint8_t seq = 0 | (uart_con_last_sequence_number_seen << 4);
	uint8_t checksum = 0;

	PEARSON(checksum, UART_CON_HEADER_SIZE);
	uart_tx(UART_CONNECTION, UART_CON_HEADER_SIZE);
	PEARSON(checksum, seq);
	uart_tx(UART_CONNECTION, seq);
	uart_tx(UART_CONNECTION, checksum);
}

static void ICACHE_FLASH_ATTR uart_con_clear_rx_dma(void) {
	// In case of failures we just ignore the data completely...
	uart_con_buffer_recv_index = 0;
	uart_con_buffer_recv_expected_length = 3;

	// ... and clear out the complete rx dma buffer
	while(uart_get_rx_fifo_count(UART_CONNECTION) > 0) {
		uart_rx(UART_CONNECTION);
	}
}

static void ICACHE_FLASH_ATTR uart_con_timeout() {
	uart_con_send_next_try();
	os_timer_disarm(&timeout_timer);
	os_timer_arm(&timeout_timer, ACK_TIMEOUT_MS, 0);
}

static void ICACHE_FLASH_ATTR uart_con_loop(os_event_t *events) {
	com_poll();

	// If the uart buffer is empty, we try to get data from tfp ringbuffer
	if(uart_con_buffer_send_size == 0) {
		tfp_poll();
	}

	if(uart_con_buffer_recv_wait_for_tfp) {
		if(tfp_send(&uart_con_buffer_recv[UART_CON_INDEX_TFP_START], uart_con_buffer_recv_expected_length-3)) {
			uart_con_buffer_recv_index = 0;
			uart_con_buffer_recv_expected_length = 3;
			uart_con_buffer_recv_wait_for_tfp = false;
		}

		system_os_post(uart_con_prio, 0, 0);
		return;
	}

	while(uart_get_rx_fifo_count(UART_CONNECTION) > 0) {
		uart_con_buffer_recv[uart_con_buffer_recv_index] = uart_rx(UART_CONNECTION);

		if(uart_con_buffer_recv_index == UART_CON_INDEX_LENGTH) {
			// TODO: Sanity-check length
			uart_con_buffer_recv_expected_length = uart_con_buffer_recv[UART_CON_INDEX_LENGTH];

			if(uart_con_buffer_recv_expected_length > UART_CON_BUFFER_SIZE) {
				logw("Length is malformed: %d > %d\n", uart_con_buffer_recv_expected_length, UART_CON_BUFFER_SIZE);
				uart_con_clear_rx_dma();
			}
		}

		uart_con_buffer_recv_index++;
		if(uart_con_buffer_recv_index >= uart_con_buffer_recv_expected_length) {
			uint8_t checksum_calculated = 0;
			for(uint8_t i = 0; i < uart_con_buffer_recv_index-1; i++) {
				PEARSON(checksum_calculated, uart_con_buffer_recv[i]);
			}

			const uint8_t checksum_uart = uart_con_buffer_recv[uart_con_buffer_recv_index-1];
			if(checksum_calculated != checksum_uart) {
				logw("Checksum error: %d (calc) != %d (uart)\n", checksum_calculated, checksum_uart);
				logw("uart recv data: %d %d [", uart_con_buffer_recv_index, uart_con_buffer_recv_expected_length);
				for(uint8_t i = 0; i < uart_con_buffer_recv_expected_length; i++) {
					logwohw("%d ", uart_con_buffer_recv[i]);
				}
				logwohw("]\n");

				uart_con_clear_rx_dma();
				break;
			}

			if(uart_con_buffer_recv_expected_length != 3) {
				const uint8_t seq = uart_con_buffer_recv[UART_CON_INDEX_SEQUENCE] & 0x0F;
				if(uart_con_last_sequence_number_seen != seq) {
					if(!tfp_send(&uart_con_buffer_recv[UART_CON_INDEX_TFP_START], uart_con_buffer_recv_expected_length-3)) {
						uart_con_buffer_recv_wait_for_tfp = true;
					}
					uart_con_last_sequence_number_seen = seq;
				}

				ack_last_message();
			}

			const uint8_t seq_seen_by_master = uart_con_buffer_recv[UART_CON_INDEX_SEQUENCE] >> 4;
			if(wait_for_ack) {
				if(seq_seen_by_master == uart_con_sequence_number) {
					os_timer_disarm(&timeout_timer);
					wait_for_ack = false;
					uart_con_sequence_number++;
					if(uart_con_sequence_number >= 0x10) {
						uart_con_sequence_number = 1;
					}
					uart_con_buffer_send_size = 0;
				}
			}

			if(!uart_con_buffer_recv_wait_for_tfp) {
				uart_con_buffer_recv_index = 0;
				uart_con_buffer_recv_expected_length = 3;
			} else {
				// If we have to wait for tfp, we need to break out of the while loop.
				break;
			}
		}
	}

//	os_timer_arm(&test_timer, 2, 0);
    system_os_post(uart_con_prio, 0, 0);
}

// Try to send message. Buffer is zeroed if sequence number is received.
// Otherwise we try again after timeout.
static void ICACHE_FLASH_ATTR uart_con_send_next_try(void) {
	uint8_t checksum = 0;
	if(uart_con_buffer_send_size > 0) {
		const uint8_t length = uart_con_buffer_send_size + 3;
		const uint8_t seq    = uart_con_sequence_number | (uart_con_last_sequence_number_seen << 4);
		PEARSON(checksum, length);
		uart_tx(UART_CONNECTION, length);
		PEARSON(checksum, seq);
		uart_tx(UART_CONNECTION, seq);
		for(uint8_t i = 0; i < uart_con_buffer_send_size; i++) {
			PEARSON(checksum, uart_con_buffer_send[i]);
			uart_tx(UART_CONNECTION, uart_con_buffer_send[i]);
		}
		uart_tx(UART_CONNECTION, checksum);

		os_timer_disarm(&timeout_timer);
		os_timer_arm(&timeout_timer, ACK_TIMEOUT_MS, 0);
		wait_for_ack = true;
	}
}

uint8_t ICACHE_FLASH_ATTR uart_con_send(const uint8_t *data, const uint8_t length) {
	if(uart_con_buffer_send_size == 0) {
		for(uint8_t i = 0; i < length; i++) {
			uart_con_buffer_send[i] =  data[i];
		}
		uart_con_buffer_send_size = length;
		uart_con_send_next_try();
		return length;
	}

	return 0;
}

void ICACHE_FLASH_ATTR uart_con_init(void) {
	//uart_swap_uart0();
	uart_configure(UART_CONNECTION, 1000000, UART_PARITY_NONE, UART_STOP_ONE, UART_DATA_EIGHT);
	uart_enable(UART_CONNECTION);

	os_timer_disarm(&timeout_timer);
	os_timer_setfn(&timeout_timer, (os_timer_func_t *)uart_con_timeout, NULL);

    // Start os task
    system_os_task(uart_con_loop, uart_con_prio, uart_con_task, uart_con_queue_length);
    system_os_post(uart_con_prio, 0, 0);
}
