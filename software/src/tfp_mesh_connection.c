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
#include <mem.h>
#include "c_types.h"
#include "ip_addr.h"
#include "espconn.h"
#include "osapi.h"
#include "espmissingincludes.h"
#include "configuration.h"
#include "mesh.h"
#include "tfp_mesh_connection.h"

#if(MESH_ENABLED == 1)
  extern Configuration configuration_current;

  static espconn tfp_mesh_sock;
  static esp_tcp tfp_mesh_sock_tcp;

  os_timer_t tmr_tfp_mesh_stat;

  void ICACHE_FLASH_ATTR tfp_mesh_open_connection(void) {
    int8_t ret = 0;

    // Clear out socket data structure storage.
    os_bzero(&tfp_mesh_sock, sizeof(tfp_mesh_sock));
    os_bzero(&tfp_mesh_sock_tcp, sizeof(tfp_mesh_sock_tcp));

    // Prepare the socket.
    tfp_mesh_sock.type = ESPCONN_TCP;
    tfp_mesh_sock.state = ESPCONN_NONE;

    // TCP parameters of the socket.
    tfp_mesh_sock_tcp.local_port = espconn_port();
    tfp_mesh_sock_tcp.remote_port = configuration_current.mesh_server_port;
    os_memcpy(tfp_mesh_sock_tcp.remote_ip, configuration_current.mesh_server_ip,
      sizeof(configuration_current.mesh_server_ip));

    tfp_mesh_sock.proto.tcp = &tfp_mesh_sock_tcp;

    // Register the connect callback.
    espconn_regist_connectcb(&tfp_mesh_sock, cb_tfp_mesh_connect);

    ret = espconn_mesh_connect(&tfp_mesh_sock);

    if(ret == ESPCONN_RTE) {
      os_printf("\n[+]MSH:Mesh server connect failed, routing problem\n");
    }
    else if(ret == ESPCONN_MEM) {
      os_printf("\n[+]MSH:Mesh server connect failed, out of memory\n");
    }
    else if(ret == ESPCONN_ISCONN) {
      os_printf("\n[+]MSH:Mesh server connect failed, already connected\n");
    }
    else if(ret == ESPCONN_ARG) {
      os_printf("\n[+]MSH:Mesh server connect failed, illegal argument\n");
    }
    else if(ret == ESPCONN_ARG) {
      os_printf("\n[+]MSH:Mesh server connect failed, routing problem\n");
    }
    else if(ret == 0) {
      os_printf("\n[+]MSH:Mesh opened connection\n");
    }
  }

  void ICACHE_FLASH_ATTR tfp_mesh_send_test_pkt(espconn *sock) {
    int i = 0;
    int8_t ret = 0;
    uint8_t data[6];
    uint8_t src_mac_addr[6];
    uint8_t dst_mac_addr[6];
    struct mesh_header_format *m_header = NULL;

    // Clearout parameter storage.
    os_bzero(data, sizeof(data));
    os_bzero(src_mac_addr, sizeof(src_mac_addr));
    os_bzero(dst_mac_addr, sizeof(dst_mac_addr));

    wifi_get_macaddr(STATION_IF, src_mac_addr);

    data[0] = 'M';
    data[1] = 'A';
    data[2] = 'C';

    os_memcpy(&data[3], &src_mac_addr[3], 3);
    os_memcpy(dst_mac_addr, configuration_current.mesh_server_ip,
      sizeof(configuration_current.mesh_server_ip));
    os_memcpy(&dst_mac_addr[4], &configuration_current.mesh_server_port,
      sizeof(configuration_current.mesh_server_port));

    m_header = (struct mesh_header_format*)espconn_mesh_create_packet
    (
      dst_mac_addr, // Destination address.
      src_mac_addr, // Source address.
      false, // Node-to-node packet (P2P).
      true, // Piggyback flow request.
      M_PROTO_BIN, // Protocol used for the payload.
      sizeof(data), // Length of the payload.
      false, // Option flag.
      0, // Optional total length.
      false, // Fragmentation flag.
      0, // Fragmentation type.
      false, // More fragmentation.
      0, // Fragmentation index.
      0 // Fragmentation ID.
    );

    if (!m_header) {
      os_printf("\n[+]MSH:Mesh packet creation failed\n");

      return;
    }

    m_header->proto.d = 1; // Upwards packet, going out of the mesh to the server.

    if (!espconn_mesh_set_usr_data(m_header, data, sizeof(data))) {
      os_printf("\n[+]MSH:Set user data failed\n");
      os_free(m_header);

      return;
    }

    /*
     * Mesh packet can be sent when the previous packet has sent. So it is advised
     * to call the following function from the sent callback of the previos packet.
     */
    ret = espconn_mesh_sent(sock, (uint8_t *)m_header, m_header->len);

    if(ret == ESPCONN_MEM) {
      os_printf("\n[+]MSH:Send fail, out of memory\n");
    }
    else if(ret == ESPCONN_ARG) {
      os_printf("\n[+]MSH:Send fail, invalid argument\n");
    }
    else if(ret == ESPCONN_MAXNUM) {
      os_printf("\n[+]MSH:Send fail, buffer overflow\n");
    }
    else if(ret == ESPCONN_IF) {
      os_printf("\n[+]MSH:Send fail, UDP data fail\n");
    }
    else if(ret == 0) {
      os_printf("\n[+]MSH:Send OK\n");
    }

    os_free(m_header);
  }

  void cb_tmr_tfp_mesh_stat(void) {
    if(espconn_mesh_is_root()) {
      os_printf("\n[+]MSH:Root node\n");
    }
    else {
      os_printf("\n[+]MSH:Not root node\n");
    }

    if(espconn_mesh_is_root_candidate()) {
      os_printf("\n[+]MSH:Root candidate\n");
    }
    else {
      os_printf("\n[+]MSH:Not root candidate\n");
    }

    if(espconn_mesh_get_status() == MESH_DISABLE) {
      os_printf("\n[+]MSH:Status=Disabled\n");
    }
    else if (espconn_mesh_get_status() == MESH_WIFI_CONN) {
      os_printf("\n[+]MSH:Status=Connecting to WiFi\n");
    }
    else if (espconn_mesh_get_status() == MESH_NET_CONN) {
      os_printf("\n[+]MSH:Status=Got IP\n");
    }
    else if (espconn_mesh_get_status() == MESH_ONLINE_AVAIL) {
      os_printf("\n[+]MSH:Status=Online\n");
    }
    else if (espconn_mesh_get_status() == MESH_LOCAL_AVAIL) {
      os_printf("\n[+]MSH:Status=Local\n");
    }

    os_printf("\n[+]MSH:Routing table\n");
    espconn_mesh_disp_route_table();
  }

  void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg) {
    espconn * sock = (espconn*)arg;

    os_printf("\n[+]MSH:CB mesh sent\n");

    if(sock != &tfp_mesh_sock) {
      os_printf("\n[+]MSH:Wrong socket\n");

      return;
    }

    tfp_mesh_send_test_pkt(sock);
  }

  void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void* arg) {
    os_printf("\n[+]MSH:Server connected\n");

    espconn *sock = (espconn *)arg;

    if (sock != &tfp_mesh_sock) {
      os_printf("\n[+]MSH:Wrong socket\n");

      return;
    }

    if(!espconn_set_opt(sock, ESPCONN_NODELAY)) {
      os_printf("\n[+]MSH:Error setting socket option ESPCONN_NODELAY\n");

      return;
    }

    if(!espconn_set_opt(sock, ESPCONN_REUSEADDR)) {
      os_printf("\n[+]MSH:Error setting socket option ESPCONN_REUSEADDR\n");

      return;
    }

    if(!espconn_set_opt(sock, ESPCONN_COPY)) {
      os_printf("\n[+]MSH:Error setting socket option ESPCONN_COPY\n");

      return;
    }

    if((espconn_regist_recvcb(sock, cb_tfp_mesh_receive)) ||
    (espconn_regist_sentcb(sock, cb_tfp_mesh_sent))) {
      espconn_mesh_disable(NULL);
      os_printf("\n[+]MSH:Error registering callbacks\n");

      return;
    }

    // Setup timer to periodically print node status.
    os_timer_disarm(&tmr_tfp_mesh_stat);
    os_timer_setfn(&tmr_tfp_mesh_stat,
      (os_timer_func_t *)cb_tmr_tfp_mesh_stat, NULL);
    os_timer_arm(&tmr_tfp_mesh_stat, 8000, true);

    /*
     * Send the first packet. Subsequent packets are sent only from the received
     * callback of the previous packet.
     */
    tfp_mesh_send_test_pkt(&tfp_mesh_sock);
  }

  void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac) {
    if (!mac) {
      return;
    }

    uint8_t *_mac = (uint8_t *)mac;

    os_printf("\n[+]MSH:CB new node join,MAC=[%x:%x:%x:%x:%x:%x]\n", _mac[0],
    _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);
  }

  void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status) {
    os_printf("\n[+]MSH:CB mesh enabled\n");

    // Create and connect the socket to the server.
    tfp_mesh_open_connection();
  }

  /*
   * Callback only called if there is payload. Not called if the received packet
   * has only a mesh header but no payload.
   */
  void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata, unsigned short len) {
    int i = 0;
    uint8_t *data = NULL;
    char data_1[3], data_2[3];
    espconn *sock = (espconn *)arg;
    struct mesh_header_format *m_header = (struct mesh_header_format *)pdata;

    if(sock != &tfp_mesh_sock) {
      os_printf("\n[+]MSH:Wrong socket\n");

      return;
    }

    if(!espconn_mesh_get_usr_data(m_header, &data, &len)) {
      os_printf("\n[+]MSH:Error getting user data\n");

      return;
    }

    os_memcpy(data_1, data, 3);
    os_memcpy(data_2, &data[3], 3);

    os_printf("\n[+]MSH:Received,DATA=%c%c%c0x%x0x%x0x%x\n", data_1[0], data_1[1],
    data_1[2], data_2[0], data_2[1], data_2[2]);
  }
#endif
