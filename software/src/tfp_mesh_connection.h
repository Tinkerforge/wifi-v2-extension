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

/*
 * 1. Connect to server IP:PORT and maintain the socket. Also register the
 * callbacks for the socket.
 *
 * 2. Put received data on the ring buffer.
 *
 * 3. Put outgoing data (going up the stack from master brick) to the send
 * function.
 *
 * 4. Dont forget to do packet counting.
 */

#if(MESH_ENABLED == 1)
  void ICACHE_FLASH_ATTR tfp_mesh_open_connection(void);
  void ICACHE_FLASH_ATTR tfp_mesh_send_test_pkt(espconn *sock);

  // Callbacks.
  void cb_tmr_tfp_mesh_stat(void);
  void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg);
  void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void* arg);
  void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac);
  void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status);
  void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata,
    unsigned short len);
#endif

#endif
