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
#include "logging.h"
#include "ringbuffer.h"
#include "tfp_mesh_connection.h"

extern Configuration configuration_current;
extern GetWifi2MeshCommonStatusReturn gw2mcsr;

// Timers.
os_timer_t tmr_tfp_mesh_enable;
static os_timer_t tmr_tfp_mesh_hb_ping;
static os_timer_t tmr_tfp_mesh_wait_pong;
static os_timer_t tmr_tfp_mesh_wait_olleh;

static esp_tcp tfp_mesh_sock_tcp;
static TFPConnection tfp_con_mesh;
static Ringbuffer tfp_mesh_send_rb;
static struct espconn tfp_mesh_sock;
static uint8_t dst_mac_addr_to_gw[ESP_MESH_ADDR_LEN];
static uint8_t src_mac_addr_to_gw[ESP_MESH_ADDR_LEN];
static uint8_t tfp_mesh_send_rb_buffer[TFP_MESH_SEND_RING_BUFFER_SIZE];

void ICACHE_FLASH_ATTR init_tfp_con_mesh(void) {
  os_bzero(&tfp_con_mesh, sizeof(TFPConnection));

  tfp_con_mesh.recv_buffer_index           = 0;
  tfp_con_mesh.recv_buffer_expected_length = TFP_MESH_MIN_LENGTH;
  tfp_con_mesh.state                       = TFP_CON_STATE_CLOSED;
  tfp_con_mesh.brickd_authentication_state = BRICKD_AUTHENTICATION_STATE_DISABLED;
  tfp_con_mesh.websocket_state             = WEBSOCKET_STATE_NO_WEBSOCKET;
  tfp_con_mesh.cid                         = 0;
  tfp_con_mesh.con                         = NULL;
}

void ICACHE_FLASH_ATTR tfp_mesh_open_connection(void) {
  int8_t ret = 0;

  logi("MSH:Opening connection...\n");

  os_bzero(&tfp_mesh_sock, sizeof(tfp_mesh_sock));
  os_bzero(&tfp_mesh_sock_tcp, sizeof(tfp_mesh_sock_tcp));

  /*
   * FIXME: struct espconn's reverse field can't be retrived in callbacks.
   * The reason for this is uncertain but a guess is that it is because the mesh
   * layer does something as the callbacks in mesh mode are called from the mesh
   * layer.
   */

  // Initialise socket parameters.
  tfp_mesh_sock.reverse = NULL;
  tfp_mesh_sock.type = ESPCONN_TCP;
  tfp_mesh_sock.state = ESPCONN_NONE;
  tfp_mesh_sock_tcp.local_port = espconn_port();
  tfp_mesh_sock_tcp.remote_port = configuration_current.mesh_gateway_port;

  os_memcpy(tfp_mesh_sock_tcp.remote_ip,
            configuration_current.mesh_gateway_ip,
            sizeof(configuration_current.mesh_gateway_ip));

  tfp_mesh_sock.proto.tcp = &tfp_mesh_sock_tcp;

  /*
   * Register connect and disconnect callbacks.
   *
   * All the other callbacks are registered from the connect callback, as the
   * connect callback will provide the final socket for communication.
   */
  if(espconn_regist_connectcb(&tfp_mesh_sock, cb_tfp_mesh_connect) != 0) {
    loge("MSH:Failed to register connect callback, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  if(espconn_regist_disconcb(&tfp_mesh_sock, cb_tfp_mesh_disconnect) != 0) {
    loge("MSH:Failed to register disconnect callback, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  ret = espconn_mesh_connect(&tfp_mesh_sock);

  if(ret == ESPCONN_RTE) {
    loge("MSH:Connect failed, ESPCONN_RTE\n");
  }
  else if(ret == ESPCONN_MEM) {
    loge("MSH:Connect failed, ESPCONN_MEM\n");
  }
  else if(ret == ESPCONN_ISCONN) {
    loge("MSH:Connect failed, ESPCONN_ISCONN\n");
  }
  else if(ret == ESPCONN_ARG) {
    loge("MSH:Connect failed, ESPCONN_ARG\n");
  }
  else if(ret == 0) {
    logi("MSH:Opened connection\n");
  }
  else {
    loge("MSH:Connect failed, UNKNOWN\n");
  }
}

void ICACHE_FLASH_ATTR tfp_mesh_pong_recv_handler(void) {
  os_timer_disarm(&tmr_tfp_mesh_hb_ping);
  os_timer_disarm(&tmr_tfp_mesh_wait_pong);

  // Arm the ping timer.
  tfp_mesh_arm_timer(&tmr_tfp_mesh_hb_ping,
                     TIME_HB_PING,
                     false,
                     cb_tmr_tfp_mesh_hb_ping,
                     NULL);
}

void ICACHE_FLASH_ATTR tfp_mesh_send_buffer_clear(void) {
  if(ringbuffer_is_empty(&tfp_mesh_send_rb)) {
    return;
  }

  tfp_mesh_send_rb.start = tfp_mesh_send_rb.end;
}

void ICACHE_FLASH_ATTR tfp_mesh_disarm_all_timers(void) {
  os_timer_disarm(&tmr_tfp_mesh_enable);
  os_timer_disarm(&tmr_tfp_mesh_hb_ping);
  os_timer_disarm(&tmr_tfp_mesh_wait_pong);
  os_timer_disarm(&tmr_tfp_mesh_wait_olleh);
}

bool ICACHE_FLASH_ATTR tfp_mesh_olleh_recv_handler(void) {
  tfp_packet_header_t stack_enum_pkt;
  os_bzero(&stack_enum_pkt, sizeof(stack_enum_pkt));

  tfp_mesh_disarm_all_timers();

  tfp_con_mesh.state = TFP_CON_STATE_OPEN;

  // FIXME: UID of the master must be set properly.

  // Send stack enumerate packet to the master.
  stack_enum_pkt.function_id = FID_MASTER_STACK_ENUMERATE;
  stack_enum_pkt.length = 8;
  stack_enum_pkt.sequence_number_and_options = 16; // Sequence number = 1.

  logi("MSH:Sending stack enumerate packet to the stack master\n");

  // CID = -2 is for UART.
  com_send((void *)&stack_enum_pkt, 8, -2);

  // Arm the ping timer.
  tfp_mesh_arm_timer(&tmr_tfp_mesh_hb_ping,
                     TIME_HB_PING,
                     false,
                     cb_tmr_tfp_mesh_hb_ping,
                     NULL);

  return true;
}

void ICACHE_FLASH_ATTR tfp_mesh_arm_timer(os_timer_t *timer,
                                          uint32_t time_ms,
                                          bool repeat,
                                          os_timer_func_t *cb_func,
                                          void *arg_cb_func) {
  os_timer_disarm(timer);
  os_timer_setfn(timer, (os_timer_func_t *)cb_func, arg_cb_func);
  os_timer_arm(timer, time_ms, repeat);
}

bool ICACHE_FLASH_ATTR tfp_mesh_send_buffer_check(uint8_t len) {
  if(ringbuffer_get_free(&tfp_mesh_send_rb) < len) {
    return false;
  }
  else {
    return true;
  }
}

bool ICACHE_FLASH_ATTR tfp_mesh_reset_recv_handler(void) {
  tfp_packet_header_t stack_reset_pkt;
  os_bzero(&stack_reset_pkt, sizeof(stack_reset_pkt));

  // FIXME: UID of the master must be set properly.

  // Send stack enumerate packet to the master.
  stack_reset_pkt.function_id = FID_MASTER_RESET;
  stack_reset_pkt.length = 8;
  stack_reset_pkt.sequence_number_and_options = 16; // Sequence number = 1.

  logi("MSH:Sending stack reset packet to the stack master\n");

  // CID = -2 is for UART.
  com_send((void *)&stack_reset_pkt, 8, -2);

  return true;
}

// Invoked only when the node is a root node.
void ICACHE_FLASH_ATTR tfp_mesh_ping_recv_handler(pkt_mesh_hb_t *pkt_mesh_hb) {
  os_memcpy(&pkt_mesh_hb->header.dst_addr,
            &pkt_mesh_hb->header.src_addr,
            sizeof(pkt_mesh_hb->header.src_addr));

  os_memcpy(&pkt_mesh_hb->header.src_addr,
            &pkt_mesh_hb->header.src_addr,
            sizeof(pkt_mesh_hb->header.dst_addr));

  pkt_mesh_hb->header.proto.d = MESH_ROUTE_UPWARDS;
  pkt_mesh_hb->type = MESH_PACKET_HB_PONG;

  tfp_mesh_send(tfp_con_mesh.con, (uint8_t *)pkt_mesh_hb, pkt_mesh_hb->header.len);

  logi("MSH:Pong packet sent to the mesh gateway\n");
}

// Handler when the received packet is a TFP packet.
bool ICACHE_FLASH_ATTR tfp_mesh_tfp_recv_handler(pkt_mesh_tfp_t *pkt_mesh_tfp) {
  if(!com_handle_message((uint8_t *)&pkt_mesh_tfp->pkt_tfp,
                         pkt_mesh_tfp->pkt_tfp.header.length,
                         tfp_con_mesh.cid)) {
    brickd_route_from((void *)&pkt_mesh_tfp->pkt_tfp, tfp_con_mesh.cid);
    tfp_handle_packet((uint8_t *)&pkt_mesh_tfp->pkt_tfp,
                      pkt_mesh_tfp->pkt_tfp.header.length);
  }

  logd("MSH:Handled TFP packet\n");

  return true;
}

/*
 * This function is used to send packets with mesh header.
 * It is assumed that the provided arguments point to a valid mesh packet.
 */
int8_t ICACHE_FLASH_ATTR tfp_mesh_send(void *arg, uint8_t *data, uint16_t length) {
  int8_t ret = 0;
  struct mesh_header_format *m_header = NULL;
  struct espconn *sock = (struct espconn *)arg;

  if(tfp_con_mesh.state == TFP_CON_STATE_OPEN) {
    tfp_con_mesh.state = TFP_CON_STATE_SENDING;

    /*
     * Just like mesh receive, the mesh send function will not send unless there is
     * a packet with valid mesh header and non-zero payload.
     */
    ret = espconn_mesh_sent(sock, data, length);

    if(ret == 0) {
      m_header = (struct mesh_header_format *)data;

      logd("MSH:Send OK (T: %d, L: %d)\n",
           data[sizeof(struct mesh_header_format)],
           m_header->len);
    }
    else if(ret == ESPCONN_MEM) {
      loge("MSH:Send error, ESPCONN_MEM\n");
    }
    else if(ret == ESPCONN_ARG) {
      loge("MSH:Send error, ESPCONN_ARG\n");
    }
    else if(ret == ESPCONN_MAXNUM) {
      loge("MSH:Send error, ESPCONN_MAXNUM\n");
    }
    else if(ret == ESPCONN_IF) {
      loge("MSH:Send error, ESPCONN_IF\n");
    }

    return ret;
  }
  else if(tfp_con_mesh.state == TFP_CON_STATE_SENDING) {
   // Check if there is enough space in the send buffer.
   logd("MSH:Can't send now, buffering (L: %d)...\n", length);

   if(tfp_mesh_send_buffer_check(length)) {
     // Store the packet in the TFP mesh send buffer.
     for(uint32_t i = 0; i < length; i++) {
       if(!ringbuffer_add(&tfp_mesh_send_rb, data[i])) {
         loge("MSH:Buffering failed, could not add byte\n");

         tfp_mesh_reset_recv_handler();
       }
     }
     return 0;
   }
   else {
     loge("MSH:Buffering failed, buffer full\n");

     tfp_mesh_reset_recv_handler();
   }
  }
  else {
   loge("MSH:Send failed, socket state\n");

   tfp_mesh_reset_recv_handler();
  }
}

/*
 * This function is called from the TFP layer below to send TFP packets
 * through mesh layer.
 */
int8_t ICACHE_FLASH_ATTR tfp_mesh_send_handler(const uint8_t *data, uint8_t length) {
  int8_t ret = 0;
  pkt_mesh_tfp_t tfp_mesh_pkt;
  struct mesh_header_format *m_header = NULL;

  os_bzero(&tfp_mesh_pkt, sizeof(pkt_mesh_tfp_t));

  m_header = (struct mesh_header_format*)espconn_mesh_create_packet
  (
    dst_mac_addr_to_gw, // Destination address.
    src_mac_addr_to_gw, // Source address.
    false, // Node-to-node packet (P2P).
    true, // Piggyback flow request.
    M_PROTO_BIN, // Protocol used for the payload.
    length + 1, // Length of payload.
    false, // Option flag.
    0, // Optional total length.
    false, // Fragmentation flag.
    0, // Fragmentation type.
    false, // More fragmentation.
    0, // Fragmentation index.
    0 // Fragmentation ID.
  );

  m_header->proto.d = MESH_ROUTE_UPWARDS;

  os_memcpy(&tfp_mesh_pkt.header, m_header, sizeof(struct mesh_header_format));
  os_free(m_header);

  tfp_mesh_pkt.type = MESH_PACKET_TFP;
  os_memcpy(&tfp_mesh_pkt.pkt_tfp, data, length);

  ret = tfp_mesh_send(tfp_con_mesh.con, (uint8_t*)&tfp_mesh_pkt, tfp_mesh_pkt.header.len);

  return ret;
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg) {
  uint16_t tfp_mesh_send_packet_len = 0;
  struct mesh_header_format *m_header = NULL;
  uint8_t tfp_mesh_send_packet[TFP_MESH_MAX_LENGTH];

  // Update mesh sent packet count.
  gw2mcsr.tx_count++;

  /*
   * If the callback is for recently sent mesh hello packet then
   * continue no further.
   */
  if(tfp_con_mesh.state == TFP_CON_STATE_MESH_SENT_HELLO) {
    logi("MSH:Done sending hello, waiting for olleh...\n");

    return;
  }

  // Update socket state.
  tfp_con_mesh.state = TFP_CON_STATE_OPEN;

  // If TFP mesh send buffer is empty then nothing to do.
  if(ringbuffer_is_empty(&tfp_mesh_send_rb)) {
    return;
  }

  // Send packet from the TFP send buffer and remove the packet from the buffer.

  os_bzero(tfp_mesh_send_packet, sizeof(tfp_mesh_send_packet));

  // Read mesh header of the packet present in the buffer.
  if(ringbuffer_peak(&tfp_mesh_send_rb,
                     tfp_mesh_send_packet,
                     sizeof(struct mesh_header_format)) != sizeof(struct mesh_header_format)) {
    loge("MSH:Failed to peak for mesh header of the packet on send buffer, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  // Get the length field of the mesh header.
  m_header = (struct mesh_header_format *)&tfp_mesh_send_packet;
  tfp_mesh_send_packet_len = m_header->len;

  /*
   * Now that the length of the packet is known try to get the whole packet
   * from the TFP mesh send buffer for sending.
   */
  if(ringbuffer_peak(&tfp_mesh_send_rb,
                     tfp_mesh_send_packet,
                     tfp_mesh_send_packet_len) != tfp_mesh_send_packet_len) {
    loge("MSH:Failed to peak for packet from send buffer, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  logd("MSH:Sending from buffer...\n");

  tfp_mesh_send(tfp_con_mesh.con, tfp_mesh_send_packet, tfp_mesh_send_packet_len);
  ringbuffer_remove(&tfp_mesh_send_rb, tfp_mesh_send_packet_len);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void *arg) {
  logi("MSH:Node established connection\n");

  int8_t ret = 0;
  pkt_mesh_hello_t mesh_hello_pkt;
  struct mesh_header_format *m_header = NULL;
  struct espconn *sock = (struct espconn *)arg;

  if(!espconn_set_opt(sock, ESPCONN_NODELAY)) {
    loge("MSH:Failed to set socket parameter, ESPCONN_NODELAY, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  // Register callbacks of the socket.
  if(espconn_regist_reconcb(sock, cb_tfp_mesh_reconnect) != 0) {
    loge("MSH:Failed to register reconnect callback, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  if(espconn_regist_recvcb(sock, cb_tfp_mesh_receive) != 0) {
    loge("MSH:Failed to register receive callback, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  if(espconn_regist_sentcb(sock, cb_tfp_mesh_sent) != 0) {
    loge("MSH:Failed to register sent callback, resetting...\n");

    tfp_mesh_reset_recv_handler();

    return;
  }

  init_tfp_con_mesh();
  tfp_mesh_send_buffer_clear();

  tfp_con_mesh.con = sock;

  ringbuffer_init(&tfp_mesh_send_rb,
                  tfp_mesh_send_rb_buffer,
                  sizeof(tfp_mesh_send_rb_buffer));

  // Set source and destination address for packets destined to the mesh gateway.
  os_bzero(dst_mac_addr_to_gw, sizeof(dst_mac_addr_to_gw));
  os_bzero(src_mac_addr_to_gw, sizeof(src_mac_addr_to_gw));

  wifi_get_macaddr(STATION_IF, src_mac_addr_to_gw);
  os_memcpy(dst_mac_addr_to_gw, configuration_current.mesh_gateway_ip,
    sizeof(configuration_current.mesh_gateway_ip));
  os_memcpy(&dst_mac_addr_to_gw[4], &configuration_current.mesh_gateway_port,
    sizeof(configuration_current.mesh_gateway_port));

  os_bzero(&mesh_hello_pkt, sizeof(mesh_hello_pkt));

  m_header = (struct mesh_header_format*)espconn_mesh_create_packet
  (
    dst_mac_addr_to_gw, // Destination address.
    src_mac_addr_to_gw, // Source address.
    false, // Node-to-node packet (P2P).
    true, // Piggyback flow request.
    M_PROTO_BIN, // Protocol used for the payload.
    sizeof(mesh_hello_pkt) - sizeof(struct mesh_header_format), // Length of payload.
    false, // Option flag.
    0, // Optional total length.
    false, // Fragmentation flag.
    0, // Fragmentation type.
    false, // More fragmentation.
    0, // Fragmentation index.
    0 // Fragmentation ID.
  );

  m_header->proto.d = MESH_ROUTE_UPWARDS;

  os_memcpy(&mesh_hello_pkt.mesh_header, m_header, sizeof(struct mesh_header_format));
  os_free(m_header);

  mesh_hello_pkt.type = MESH_PACKET_HELLO;

  if(espconn_mesh_is_root()) {
    mesh_hello_pkt.is_root_node = true;
  }
  else {
    mesh_hello_pkt.is_root_node = false;
  }

  mesh_hello_pkt.firmware_version[0] = FIRMWARE_VERSION_MAJOR;
  mesh_hello_pkt.firmware_version[1] = FIRMWARE_VERSION_MINOR;
  mesh_hello_pkt.firmware_version[2] = FIRMWARE_VERSION_REVISION;

  os_memcpy(&mesh_hello_pkt.prefix,
            configuration_current.mesh_ssid_prefix,
            sizeof(configuration_current.mesh_ssid_prefix));

  os_memcpy(&mesh_hello_pkt.group_id,
            configuration_current.mesh_group_id,
            sizeof(configuration_current.mesh_group_id));

  logi("MSH:Sending mesh hello packet\n");

  /*
   * tfp_mesh_send() isn't used here because at this stage we want to skip
   * socket state check while sending.
   */
  ret = espconn_mesh_sent(tfp_con_mesh.con,
                          (uint8_t *)&mesh_hello_pkt,
                          mesh_hello_pkt.mesh_header.len);

  if(ret == ESPCONN_MEM) {
    loge("MSH:Send hello error, ESPCONN_MEM, resetting...\n");

    tfp_mesh_reset_recv_handler();
  }
  else if(ret == ESPCONN_ARG) {
    loge("MSH:Send hello  error, ESPCONN_ARG, resetting...\n");

    tfp_mesh_reset_recv_handler();
  }
  else if(ret == ESPCONN_MAXNUM) {
    loge("MSH:Send hello error, ESPCONN_MAXNUM, resetting...\n");

    tfp_mesh_reset_recv_handler();
  }
  else if(ret == ESPCONN_IF) {
    loge("MSH:Send hello error, ESPCONN_IF, resetting...\n");

    tfp_mesh_reset_recv_handler();
  }
  else {
    logi("MSH:Send hello OK (T: %d, L: %d)\n",
         MESH_PACKET_HELLO,
         mesh_hello_pkt.mesh_header.len);

  // Update socket state.
  tfp_con_mesh.state = TFP_CON_STATE_MESH_SENT_HELLO;

  // Arm the wait olleh timer.
  tfp_mesh_arm_timer(&tmr_tfp_mesh_wait_olleh,
                     TIME_WAIT_OLLEH,
                     false,
                     cb_tmr_tfp_mesh_wait_olleh,
                     NULL);
  }
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac) {
  if (!mac) {
    return;
  }

  uint8_t *mac_child = (uint8_t *)mac;

  logi("MSH:New node joined (M: %02X-%02X-%02X-%02X-%02X-%02X)\n",
       mac_child[0], mac_child[1], mac_child[2],
       mac_child[3], mac_child[4], mac_child[5]);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_disconnect(void *arg) {
  logi("MSH:Connection to parent disconnected\n");

	tfp_mesh_reset_recv_handler();
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_enable(int8_t status) {
  if(status == MESH_ONLINE_SUC) {
    logi("MSH:Mesh enabled, ONLINE\n");
  }
  else if(status == MESH_LOCAL_SUC) {
    logi("MSH:Mesh enabled, LOCAL\n");
  }
  else if(status == MESH_SOFTAP_SUC) {
    logi("MSH:Mesh enabled, SOFTAP\n");
  }
  else {
    loge("MSH:Mesh enable failed, re-enabling...\n");
    espconn_mesh_enable(cb_tfp_mesh_enable, MESH_ONLINE);

    return;
  }

  tfp_mesh_open_connection();
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_reconnect(void *arg, sint8 error) {
  logi("MSH:Connection reconnect (E: %d)\n", error);

  cb_tfp_mesh_disconnect(arg);
}

/*
 * Callback only called if there is payload. Not called if the received packet
 * has only a mesh header but no payload.
 */
void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg, char *pdata, unsigned short len) {
  uint8_t *payload = NULL;
  uint16_t payload_len = 0;
  uint8_t payload_type = 0;
  struct mesh_header_format *m_header = NULL;

  // Update mesh receive packet count.
  gw2mcsr.rx_count++;

  for(uint32_t i = 0; i < len; i++) {
		tfp_con_mesh.recv_buffer[tfp_con_mesh.recv_buffer_index] = pdata[i];

		if(tfp_con_mesh.recv_buffer_index == TFP_MESH_HEADER_LENGTH_INDEX) {
			tfp_con_mesh.recv_buffer_expected_length =
        (uint16_t)tfp_con_mesh.recv_buffer[TFP_MESH_HEADER_LENGTH_INDEX - 1];
		}

		tfp_con_mesh.recv_buffer_index++;

		if(tfp_con_mesh.recv_buffer_index == tfp_con_mesh.recv_buffer_expected_length) {
      // Now we have the complete mesh packet in the receive buffer.
      m_header = (struct mesh_header_format *)tfp_con_mesh.recv_buffer;

      if(m_header == NULL) {
        loge("MSH:Receive failed, could not get mesh packet\n");

        goto MOVE_ON;
      }

      if(m_header->proto.protocol != M_PROTO_BIN) {
        loge("MSH:Receive failed, packet protocol type is not binary\n");

        goto MOVE_ON;
      }
      else {
        // Payload of a mesh packet is a TFP packet.
        payload = NULL;
        payload_len = 0;

        if(!espconn_mesh_get_usr_data(m_header, &payload, &payload_len)) {
          loge("MSH:Receive failed, could not get user data\n");

          goto MOVE_ON;
        }

        if(payload == NULL || payload_len == 0) {
          loge("MSH:Receive failed, could not get TFP packet\n");

          goto MOVE_ON;
        }
      }

      payload_type = payload[0];

      // Mesh olleh packet.
      if(payload_type == MESH_PACKET_OLLEH) {
        logi("MSH:Received olleh packet\n");

        tfp_mesh_olleh_recv_handler();
      }
      // Reset stack packet.
			else if(payload_type == MESH_PACKET_RESET) {
        logi("MSH:Received reset packet\n");

        tfp_mesh_reset_recv_handler();
      }
      // Mesh heart beat ping packet.
      else if(payload_type == MESH_PACKET_HB_PING) {
        logi("MSH:Received ping packet\n");

        tfp_mesh_ping_recv_handler((pkt_mesh_hb_t *)tfp_con_mesh.recv_buffer);
      }
      // Mesh heart beat pong packet.
      else if(payload_type == MESH_PACKET_HB_PONG) {
        logi("MSH:Received pong packet\n");

        tfp_mesh_pong_recv_handler();
      }
      // Mesh TFP packet.
      else if(payload_type == MESH_PACKET_TFP) {
        logi("MSH:Received TFP packet\n");

        tfp_mesh_tfp_recv_handler((pkt_mesh_tfp_t *)tfp_con_mesh.recv_buffer);
      }
      // Packet type is unknown.
      else {
        logi("MSH:Received unknown packet\n");
      }

      MOVE_ON:
        tfp_con_mesh.recv_buffer_index = 0;
        tfp_con_mesh.recv_buffer_expected_length = TFP_MESH_MIN_LENGTH;
		}
	}
}

void cb_tmr_tfp_mesh_enable(void *arg) {
  /*
	 * Enable mesh (drum roll...)
	 *
	 * This function must be called from within user_init() function.
	 */
  espconn_mesh_enable(cb_tfp_mesh_enable, MESH_ONLINE);
}

void cb_tmr_tfp_mesh_hb_ping(void *arg) {
	logi("MSH:Pinging the mesh gateway\n");

  pkt_mesh_hb_t pkt_mesh_hb;
  struct mesh_header_format *m_header = NULL;

  os_bzero(&pkt_mesh_hb, sizeof(pkt_mesh_hb_t));

  m_header = (struct mesh_header_format*)espconn_mesh_create_packet
  (
    dst_mac_addr_to_gw, // Destination address.
    src_mac_addr_to_gw, // Source address.
    false, // Node-to-node packet (P2P).
    true, // Piggyback flow request.
    M_PROTO_BIN, // Protocol used for the payload.
    sizeof(pkt_mesh_hb) - sizeof(struct mesh_header_format), // Length of payload.
    false, // Option flag.
    0, // Optional total length.
    false, // Fragmentation flag.
    0, // Fragmentation type.
    false, // More fragmentation.
    0, // Fragmentation index.
    0 // Fragmentation ID.
  );

  m_header->proto.d = MESH_ROUTE_UPWARDS;

  os_memcpy(&pkt_mesh_hb.header, m_header, sizeof(struct mesh_header_format));
  os_free(m_header);

  pkt_mesh_hb.type = MESH_PACKET_HB_PING;

  // Send the ping packet to the mesh gateway.
  tfp_mesh_send(tfp_con_mesh.con, (uint8_t *)&pkt_mesh_hb, pkt_mesh_hb.header.len);

  // Arm the wait pong timer.
  tfp_mesh_arm_timer(&tmr_tfp_mesh_wait_pong,
                     TIME_HB_WAIT_PONG,
                     false,
                     cb_tmr_tfp_mesh_wait_pong,
                     NULL);
}

void cb_tmr_tfp_mesh_wait_pong(void *arg) {
	logi("MSH:Wait pong timedout, resetting...\n");

  // Reset the stack
  tfp_mesh_reset_recv_handler();
}

void cb_tmr_tfp_mesh_wait_olleh(void *arg) {
  logi("MSH:Wait olleh timedout, resetting...\n");

  // Reset the stack
  tfp_mesh_reset_recv_handler();
}
