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
#include "communication.h"
#include "tfp_connection.h"
#include "uart_connection.h"
#include "logging.h"

extern Configuration configuration_current;
extern GetWifi2MeshCommonStatusReturn gw2mcsr;

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
  tfp_mesh_sock_tcp.remote_port = configuration_current.mesh_gateway_port;
  os_memcpy(tfp_mesh_sock_tcp.remote_ip, configuration_current.mesh_gateway_ip,
    sizeof(configuration_current.mesh_gateway_ip));

  tfp_mesh_sock.proto.tcp = &tfp_mesh_sock_tcp;

  // Register connect and reconnect callback.
  if(espconn_regist_connectcb(&tfp_mesh_sock, cb_tfp_mesh_connect) != 0) {
    loge("MSH:Error registering connect callback");

    return;
  }

  if(espconn_regist_reconcb(&tfp_mesh_sock, cb_tfp_mesh_reconnect) != 0) {
    loge("MSH:Error registering reconnect callback");

    return;
  }

  ret = espconn_mesh_connect(&tfp_mesh_sock);

  if(ret == ESPCONN_RTE) {
    loge("MSH:tfp_mesh_open_connection(), ESPCONN_RTE)\n");
  }
  else if(ret == ESPCONN_MEM) {
    loge("MSH:tfp_mesh_open_connection(), ESPCONN_MEM\n");
  }
  else if(ret == ESPCONN_ISCONN) {
    loge("MSH:tfp_mesh_open_connection(), ESPCONN_ISCONN\n");
  }
  else if(ret == ESPCONN_ARG) {
    loge("MSH:tfp_mesh_open_connection(), ESPCONN_ARG\n");
  }
  else if(ret == 0) {
    logi("MSH:Mesh opened connection\n");
  }
  else {
    logw("MSH:tfp_mesh_open_connection(), UNKNOWN\n");
  }
}

int8_t ICACHE_FLASH_ATTR tfp_mesh_send(void *arg, uint8_t *data, uint8_t length) {
  int8_t ret = 0;
  uint8_t dst_mac_addr[6];
  uint8_t src_mac_addr[6];
  struct mesh_header_format *m_header = NULL;

  uint16_t length_u16 = length;
  espconn *sock = (espconn *)arg;

  if(sock != &tfp_mesh_sock) {
    loge("MSH:tfp_mesh_send(), wrong socket\n");

    return ESPCONN_ARG;
  }

  os_bzero(dst_mac_addr, sizeof(dst_mac_addr));
  os_bzero(src_mac_addr, sizeof(src_mac_addr));

  wifi_get_macaddr(STATION_IF, src_mac_addr);
  os_memcpy(dst_mac_addr, configuration_current.mesh_gateway_ip,
    sizeof(configuration_current.mesh_gateway_ip));
  os_memcpy(&dst_mac_addr[4], &configuration_current.mesh_gateway_port,
    sizeof(configuration_current.mesh_gateway_port));

  m_header = (struct mesh_header_format*)espconn_mesh_create_packet
  (
    dst_mac_addr, // Destination address.
    src_mac_addr, // Source address.
    false, // Node-to-node packet (P2P).
    true, // Piggyback flow request.
    M_PROTO_BIN, // Protocol used for the payload.
    length_u16, // Length of the payload.
    false, // Option flag.
    0, // Optional total length.
    false, // Fragmentation flag.
    0, // Fragmentation type.
    false, // More fragmentation.
    0, // Fragmentation index.
    0 // Fragmentation ID.
  );

  if (!m_header) {
    loge("MSH:tfp_mesh_send(), mesh packet creation failed\n");

    return ESPCONN_ARG;
  }

  m_header->proto.d = 1; // Upwards packet, going out of the mesh to the server.

  if (!espconn_mesh_set_usr_data(m_header, data, length_u16)) {
    loge("MSH:tfp_mesh_send(), set user data failed\n");

    os_free(m_header);

    return ESPCONN_ARG;
  }

  /*
   * Just like mesh receive, the mesh send function will not send unless there is
   * a packet with valid mesh header and non-zero payload.
   */
  ret = espconn_mesh_sent(sock, (uint8_t *)m_header, m_header->len);

  if(ret == ESPCONN_MEM) {
    loge("MSH:tfp_mesh_send(), ESPCONN_MEM\n");
  }
  else if(ret == ESPCONN_ARG) {
    loge("MSH:tfp_mesh_send(), ESPCONN_ARG\n");
  }
  else if(ret == ESPCONN_MAXNUM) {
    loge("MSH:tfp_mesh_send(), ESPCONN_MAXNUM\n");
  }
  else if(ret == ESPCONN_IF) {
    loge("MSH:tfp_mesh_send(), ESPCONN_IF\n");
  }

  os_free(m_header);

  return ret;
}

void cb_tmr_tfp_mesh_stat(void) {
  int8_t status_node = espconn_mesh_get_status();

  logi("MSH:=== Node Status ===\n");

  if(espconn_mesh_is_root()) {
    os_printf("R:IS\n");
  }
  else {
    os_printf("R:NOT\n");
  }

  if(status_node == MESH_DISABLE) {
    os_printf("S:DISABLED\n");
  }
  else if (status_node == MESH_WIFI_CONN) {
    os_printf("S:CONNECTING\n");
  }
  else if (status_node == MESH_NET_CONN) {
    os_printf("S:GOT IP\n");
  }
  else if (status_node == MESH_LOCAL_AVAIL) {
    os_printf("S:LOCAL\n");
  }
  else if (status_node == MESH_ONLINE_AVAIL) {
    os_printf("S:ONLINE\n");
  }
  else if (status_node == MESH_SOFTAP_AVAIL) {
    os_printf("S:SOFTAP AVAILABLE\n");
  }
  else if (status_node == MESH_SOFTAP_SETUP) {
    os_printf("S:SOFTAP SETUP\n");
  }
  else if (status_node == MESH_LEAF_AVAIL) {
    os_printf("S:LEAF AVAILABLE\n");
  }
  else {
    os_printf("S:UNKNOWN\n");
  }

  if(espconn_mesh_get_sub_dev_count() > 0) {
    espconn_mesh_disp_route_table();
    os_printf("\n");
  }
  else {
    os_printf("ROUTING TABLE EMPTY\n\n");
  }
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg) {
  espconn * sock = (espconn*)arg;

  if(sock != &tfp_mesh_sock) {
    loge("MSH:cb_tfp_mesh_sent(), wrong socket\n");

    return;
  }

  /*
   * FIXME: Since the mesh implementation is more like a layer on top of the
   * existing TFP implementation maybe it is better to use the existing packet
   * count mechanism of the TFP implementation ?
   */
  gw2mcsr.tx_count++;

  tfp_sent_callback(arg);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void *arg) {
  logi("MSH:Server connected\n");

  espconn *sock = (espconn *)arg;

  if (sock != &tfp_mesh_sock) {
    loge("MSH:cb_tfp_mesh_connect(), wrong socket\n");

    espconn_mesh_disable(NULL);

    return;
  }

  if(!espconn_set_opt(sock, ESPCONN_NODELAY)) {
    loge("MSH:cb_tfp_mesh_connect(), error setting ESPCONN_NODELAY\n");

    espconn_mesh_disable(NULL);

    return;
  }

  if(espconn_regist_recvcb(sock, cb_tfp_mesh_receive) != 0) {
    loge("MSH:cb_tfp_mesh_connect(), error registering receive callback");

    espconn_mesh_disable(NULL);

    return;
  }

  if(espconn_regist_sentcb(sock, cb_tfp_mesh_sent) != 0) {
    loge("MSH:cb_tfp_mesh_connect(), error registering sent callback");

    espconn_mesh_disable(NULL);

    return;
  }

  if(espconn_regist_disconcb(sock, cb_tfp_mesh_disconnect) != 0) {
    loge("MSH:cb_tfp_mesh_connect(), error registering disconnect callback");

    espconn_mesh_disable(NULL);

    return;
  }

  // Setup timer to periodically print node status.
  os_timer_disarm(&tmr_tfp_mesh_stat);
  os_timer_setfn(&tmr_tfp_mesh_stat, (os_timer_func_t *)cb_tmr_tfp_mesh_stat, NULL);
  os_timer_arm(&tmr_tfp_mesh_stat, 8000, true);

  // Call connect callback of TFP layer.
  tfp_connect_callback(arg);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac) {
  if (!mac) {
    return;
  }

  uint8_t *_mac = (uint8_t *)mac;

  logi("MSH:cb_tfp_mesh_new_node(), MAC=[%x:%x:%x:%x:%x:%x]\n", _mac[0], _mac[1],
    _mac[2], _mac[3], _mac[4], _mac[5]);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_disconnect(void *arg) {
  logi("MSH:cb_tfp_mesh_disconnect()\n");

  tfp_disconnect_callback(arg);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status) {
  if(status == MESH_ONLINE_SUC) {
    logi("MSH:Mesh enabled, ONLINE\n");
  }
  else if(status == MESH_LOCAL_SUC) {
    logi("MSH:Mesh enabled, LOCAL\n");
  }
  else if(status == MESH_DISABLE_SUC) {
    logi("MSH:Mesh disabled\n");
  }
  else if(status == MESH_SOFTAP_SUC) {
    logi("MSH:Mesh enabled, SOFTAP\n");
  }
  else {
    loge("MSH:Mesh enable failed, re-enabling...\n");

    espconn_mesh_enable(cb_tfp_mesh_enable, MESH_ONLINE);

    return;
  }

  // Create and connect the socket to the server.
  tfp_mesh_open_connection();
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_reconnect(void *arg, sint8 error) {
  logi("MSH:cb_tfp_mesh_reconnect()\n");

  tfp_reconnect_callback(arg, error);
}

/*
 * Callback only called if there is payload. Not called if the received packet
 * has only a mesh header but no payload.
 */
void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata, uint16_t len) {
  int i = 0;
  uint8_t *pkt_tfp = NULL;
  uint16_t pkt_tfp_len = 0;
  espconn *sock = (espconn *)arg;
  struct mesh_header_format *m_header = (struct mesh_header_format *)pdata;

  if(sock != &tfp_mesh_sock) {
    loge("MSH:cb_tfp_mesh_receive(), wrong socket\n");

    return;
  }

  // Payload of a mesh packet is a TFP packet.
  if(!espconn_mesh_get_usr_data(m_header, &pkt_tfp, &pkt_tfp_len)) {
    loge("MSH:cb_tfp_mesh_receive(), error getting user data\n");

    return;
  }

  /*
   * FIXME: Since the mesh implementation is more like a layer on top of the
   * existing TFP implementation maybe it is better to use the existing packet
   * count mechanism of the TFP implementation ?
   */
  gw2mcsr.rx_count++;

  /*
   * TODO: For test try putting a TFP packet to he receive callback of
   * the TFP implementation.
   */
  // UID:6CRh52, Master Status LED Enable.
  //uint8_t s_led_on[8] = {0x31, 0x17, 0x77, 0xDC, 0x08, 0xEE, 0x18/*0x10*/, 0x00};
  // UID:6CRh52, Master Status LED Disable.
  //uint8_t s_led_off[8] = {0x31, 0x17, 0x77, 0xDC, 0x08, 0xEF, 0x18/*0x10*/, 0x00};
  //tfp_recv_callback(arg, (char *)s_led_on, 8);
  //tfp_recv_callback(arg, (char *)s_led_off, 8);

  tfp_recv_callback(arg, (char *)pkt_tfp, (unsigned short)pkt_tfp_len);
}
