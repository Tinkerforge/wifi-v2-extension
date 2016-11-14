/* WIFI Extension 2.0
 * Copyright (C) 2016 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * configuration.h: Mesh networking implementation
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

#ifndef TFP_MESH_CONNECTION_H
#define TFP_MESH_CONNECTION_H

#include "c_types.h"
#include "user_interface.h"

// These parameters will come from loaded configuration.
#define TFP_MESH_MAX_HOP 4

#define TFP_MESH_ROUTER_SSID "Tinkerforge WLAN"
#define TFP_MESH_ROUTER_SSID_PASSWORD "25842149320894763607"

#define TFP_MESH_NODE_SSID_PREFIX "TF_MESH_DEV_NODE"
#define TFP_MESH_NODE_SSID_AUTHENTICATION AUTH_WPA_WPA2_PSK
#define TFP_MESH_NODE_SSID_PASSWORD "1234567890"

extern uint8_t TFP_MESH_SERVER_IP[4];
extern uint16_t TFP_MESH_SERVER_PORT;

extern uint8_t TFP_MESH_GROUP_ID[6];
extern uint8_t TFP_MESH_ROUTER_BSSID[6];

extern os_timer_t tmr_tfp_mesh_stat;
extern os_timer_t tmr_tfp_mesh_test_send;

void cb_tmr_tfp_mesh_stat(void);
void cb_tmr_tfp_mesh_test_send(void);

/*
 * 1. Connect to server IP:PORT and maintain the socket. Also register the callbacks for the socket.
 *
 * 2. Put received data on the ring buffer.
 *
 * 3. Put outgoing data (going up the stack from master brick) to the send function (queue?).
 *
 * 4. Dont forget to do packet counting.
 */
void ICACHE_FLASH_ATTR tfp_mesh_enable(void);
void ICACHE_FLASH_ATTR tfp_mesh_open_connection(void);
bool ICACHE_FLASH_ATTR tfp_mesh_send(const uint8_t *data, const uint8_t length);

// Callbacks.
void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status);
void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void* arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg);
void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata, unsigned short len);
void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac);

void cb_tmr_tfp_mesh_stat(void);
void ICACHE_FLASH_ATTR tfp_mesh_send_test_pkt(espconn *sock);

#endif
