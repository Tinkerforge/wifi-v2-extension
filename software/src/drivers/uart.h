/* ESP8266 drivers
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * uart.h: Implementation of uart access
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

#ifndef UART_H
#define UART_H

#include "c_types.h"

#define UART0   0
#define UART1   1


// Register definitions from espressif uart_register.h

typedef enum {
    UART_DATA_FIVE  = 0,
    UART_DATA_SIX   = 1,
    UART_DATA_SEVEN = 2,
    UART_DATA_EIGHT = 3
} UartDataBits;

typedef enum {
	UART_STOP_ONE      = 0,
    UART_STOP_ONE_HALF = 1,
    UART_STOP_TWO      = 2
} UartStopBits;

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_ODD  = 1,
    UART_PARITY_EVEN = 2
} UartParity;

void ICACHE_FLASH_ATTR uart_configure(uint8_t uart, uint32_t baudrate, UartParity parity, UartStopBits stop_bits, UartDataBits data_bits);
void ICACHE_FLASH_ATTR uart_swap_uart0(void);
void ICACHE_FLASH_ATTR uart_enable(uint8_t uart);
uint8_t ICACHE_FLASH_ATTR uart_get_rx_fifo_count(uint8_t uart);
uint8_t ICACHE_FLASH_ATTR uart_get_tx_fifo_count(uint8_t uart);
uint8_t ICACHE_FLASH_ATTR uart_rx(uint8_t uart);
void ICACHE_FLASH_ATTR uart_tx(uint8_t uart, uint8_t c);
void ICACHE_FLASH_ATTR uart_printf_write_char(char c);
void ICACHE_FLASH_ATTR uart_set_printf(uint8_t uart);

#define REG_UART_BASE(i)                (0x60000000 + (i)*0xf00)
//version value:32'h062000

#define UART_FIFO(i)                    (REG_UART_BASE(i) + 0x0)
#define UART_RXFIFO_RD_BYTE                 0x000000FF
#define UART_RXFIFO_RD_BYTE_S               0

#define UART_INT_RAW(i)                 (REG_UART_BASE(i) + 0x4)
#define UART_RXFIFO_TOUT_INT_RAW            (BIT(8))
#define UART_BRK_DET_INT_RAW                (BIT(7))
#define UART_CTS_CHG_INT_RAW                (BIT(6))
#define UART_DSR_CHG_INT_RAW                (BIT(5))
#define UART_RXFIFO_OVF_INT_RAW             (BIT(4))
#define UART_FRM_ERR_INT_RAW                (BIT(3))
#define UART_PARITY_ERR_INT_RAW             (BIT(2))
#define UART_TXFIFO_EMPTY_INT_RAW           (BIT(1))
#define UART_RXFIFO_FULL_INT_RAW            (BIT(0))

#define UART_INT_ST(i)                  (REG_UART_BASE(i) + 0x8)
#define UART_RXFIFO_TOUT_INT_ST             (BIT(8))
#define UART_BRK_DET_INT_ST                 (BIT(7))
#define UART_CTS_CHG_INT_ST                 (BIT(6))
#define UART_DSR_CHG_INT_ST                 (BIT(5))
#define UART_RXFIFO_OVF_INT_ST              (BIT(4))
#define UART_FRM_ERR_INT_ST                 (BIT(3))
#define UART_PARITY_ERR_INT_ST              (BIT(2))
#define UART_TXFIFO_EMPTY_INT_ST            (BIT(1))
#define UART_RXFIFO_FULL_INT_ST             (BIT(0))

#define UART_INT_ENA(i)                 (REG_UART_BASE(i) + 0xC)
#define UART_RXFIFO_TOUT_INT_ENA            (BIT(8))
#define UART_BRK_DET_INT_ENA                (BIT(7))
#define UART_CTS_CHG_INT_ENA                (BIT(6))
#define UART_DSR_CHG_INT_ENA                (BIT(5))
#define UART_RXFIFO_OVF_INT_ENA             (BIT(4))
#define UART_FRM_ERR_INT_ENA                (BIT(3))
#define UART_PARITY_ERR_INT_ENA             (BIT(2))
#define UART_TXFIFO_EMPTY_INT_ENA           (BIT(1))
#define UART_RXFIFO_FULL_INT_ENA            (BIT(0))

#define UART_INT_CLR(i)                 (REG_UART_BASE(i) + 0x10)
#define UART_RXFIFO_TOUT_INT_CLR            (BIT(8))
#define UART_BRK_DET_INT_CLR                (BIT(7))
#define UART_CTS_CHG_INT_CLR                (BIT(6))
#define UART_DSR_CHG_INT_CLR                (BIT(5))
#define UART_RXFIFO_OVF_INT_CLR             (BIT(4))
#define UART_FRM_ERR_INT_CLR                (BIT(3))
#define UART_PARITY_ERR_INT_CLR             (BIT(2))
#define UART_TXFIFO_EMPTY_INT_CLR           (BIT(1))
#define UART_RXFIFO_FULL_INT_CLR            (BIT(0))

#define UART_CLKDIV(i)                  (REG_UART_BASE(i) + 0x14)
#define UART_CLKDIV_CNT                     0x000FFFFF
#define UART_CLKDIV_S                       0

#define UART_AUTOBAUD(i)                (REG_UART_BASE(i) + 0x18)
#define UART_GLITCH_FILT                    0x000000FF
#define UART_GLITCH_FILT_S                  8
#define UART_AUTOBAUD_EN                    (BIT(0))

#define UART_STATUS(i)                  (REG_UART_BASE(i) + 0x1C)
#define UART_TXD                            (BIT(31))
#define UART_RTSN                           (BIT(30))
#define UART_DTRN                           (BIT(29))
#define UART_TXFIFO_CNT                     0x000000FF
#define UART_TXFIFO_CNT_S                   16
#define UART_RXD                            (BIT(15))
#define UART_CTSN                           (BIT(14))
#define UART_DSRN                           (BIT(13))
#define UART_RXFIFO_CNT                     0x000000FF
#define UART_RXFIFO_CNT_S                   0

#define UART_CONF0(i)                   (REG_UART_BASE(i) + 0x20)
#define UART_DTR_INV                        (BIT(24))
#define UART_RTS_INV                        (BIT(23))
#define UART_TXD_INV                        (BIT(22))
#define UART_DSR_INV                        (BIT(21))
#define UART_CTS_INV                        (BIT(20))
#define UART_RXD_INV                        (BIT(19))
#define UART_TXFIFO_RST                     (BIT(18))
#define UART_RXFIFO_RST                     (BIT(17))
#define UART_IRDA_EN                        (BIT(16))
#define UART_TX_FLOW_EN                     (BIT(15))
#define UART_LOOPBACK                       (BIT(14))
#define UART_IRDA_RX_INV                    (BIT(13))
#define UART_IRDA_TX_INV                    (BIT(12))
#define UART_IRDA_WCTL                      (BIT(11))
#define UART_IRDA_TX_EN                     (BIT(10))
#define UART_IRDA_DPLX                      (BIT(9))
#define UART_TXD_BRK                        (BIT(8))
#define UART_SW_DTR                         (BIT(7))
#define UART_SW_RTS                         (BIT(6))
#define UART_STOP_BIT_NUM                   0x00000003
#define UART_STOP_BIT_NUM_S                 4
#define UART_BIT_NUM                        0x00000003
#define UART_BIT_NUM_S                      2
#define UART_PARITY_EN                      (BIT(1))
#define UART_PARITY_EN_M                0x00000001
#define UART_PARITY_EN_S                 1
#define UART_PARITY                         (BIT(0))
#define UART_PARITY_M                       0x00000001
#define UART_PARITY_S                        0

#define UART_CONF1(i)                   (REG_UART_BASE(i) + 0x24)
#define UART_RX_TOUT_EN                     (BIT(31))
#define UART_RX_TOUT_THRHD                  0x0000007F
#define UART_RX_TOUT_THRHD_S                24
#define UART_RX_FLOW_EN                     (BIT(23))
#define UART_RX_FLOW_THRHD                  0x0000007F
#define UART_RX_FLOW_THRHD_S                16
#define UART_TXFIFO_EMPTY_THRHD             0x0000007F
#define UART_TXFIFO_EMPTY_THRHD_S           8
#define UART_RXFIFO_FULL_THRHD              0x0000007F
#define UART_RXFIFO_FULL_THRHD_S            0

#define UART_LOWPULSE(i)                (REG_UART_BASE(i) + 0x28)
#define UART_LOWPULSE_MIN_CNT               0x000FFFFF
#define UART_LOWPULSE_MIN_CNT_S             0

#define UART_HIGHPULSE(i)               (REG_UART_BASE(i) + 0x2C)
#define UART_HIGHPULSE_MIN_CNT              0x000FFFFF
#define UART_HIGHPULSE_MIN_CNT_S            0

#define UART_PULSE_NUM(i)               (REG_UART_BASE(i) + 0x30)
#define UART_PULSE_NUM_CNT                  0x0003FF
#define UART_PULSE_NUM_CNT_S                0

#define UART_DATE(i)                    (REG_UART_BASE(i) + 0x78)
#define UART_ID(i)                      (REG_UART_BASE(i) + 0x7C)

#endif
