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
#include "logging.h"

extern Configuration configuration_current;
extern GetWifi2MeshCommonStatusReturn gw2mcsr;

static esp_tcp tfp_mesh_sock_tcp;
static TFPConnection tfp_con_mesh;
static Ringbuffer tfp_mesh_send_rb;
static struct espconn tfp_mesh_sock;
static bool sent_mesh_hello = false;
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

  os_bzero(&tfp_mesh_sock, sizeof(tfp_mesh_sock));
  os_bzero(&tfp_mesh_sock_tcp, sizeof(tfp_mesh_sock_tcp));

  // Initialise socket parameters.
  tfp_mesh_sock.type = ESPCONN_TCP;
  tfp_mesh_sock.state = ESPCONN_NONE;
  tfp_mesh_sock.reverse = NULL;

  // TCP parameters of the socket.
  tfp_mesh_sock_tcp.local_port = espconn_port();
  tfp_mesh_sock_tcp.remote_port = configuration_current.mesh_gateway_port;

  os_memcpy(tfp_mesh_sock_tcp.remote_ip,
            configuration_current.mesh_gateway_ip,
            sizeof(configuration_current.mesh_gateway_ip));

  tfp_mesh_sock.proto.tcp = &tfp_mesh_sock_tcp;

  /*
   * FIXME: struct espconn's reverse field can't be retrived in callbacks.
   * The reason for this is uncertain but a guess is that it is because the mesh
   * layer does something as the callbacks in mesh mode are called from the mesh
   * layer.
   */

  /*
   * Register connect callback. All the other callbacks are registered from the
   * connect callback as connect callback will provide the final socket for
   * communication.
   */
  if(espconn_regist_connectcb(&tfp_mesh_sock, cb_tfp_mesh_connect) != 0) {
    loge("MSH:Error registering connect callback");

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
    logw("MSH:Connect failed, UNKNOWN\n");
  }
}

void ICACHE_FLASH_ATTR tfp_mesh_send_buffer_clear(void) {
  if(ringbuffer_is_empty(&tfp_mesh_send_rb)) {
    return;
  }

  tfp_mesh_send_rb.start = tfp_mesh_send_rb.end;
}

bool ICACHE_FLASH_ATTR tfp_mesh_send_buffer_check(uint8_t len) {
  if(ringbuffer_get_free(&tfp_mesh_send_rb) < len) {
    return false;
  }
  else {
    return true;
  }
}

int8_t ICACHE_FLASH_ATTR tfp_mesh_send(void *arg,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint8_t packet_type) {
  int8_t ret = 0;
  uint8_t dst_mac_addr[ESP_MESH_ADDR_LEN];
  uint8_t src_mac_addr[ESP_MESH_ADDR_LEN];
  struct mesh_header_format *m_header = NULL;
  uint8_t data_with_type[TFP_MAX_LENGTH + sizeof(struct mesh_header_format) + 1];

  if(packet_type < 1) {
    loge("MSH:Unknown packet type to send\n");

    return ESPCONN_ARG;
  }

  os_bzero(&data_with_type, sizeof(data_with_type));
  data_with_type[0] = packet_type;
  os_memcpy(&data_with_type[1], data, length);

  length++;

  // Update state of the socket.
  tfp_con_mesh.state = TFP_CON_STATE_SENDING;

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
    length, // Length of payload.
    false, // Option flag.
    0, // Optional total length.
    false, // Fragmentation flag.
    0, // Fragmentation type.
    false, // More fragmentation.
    0, // Fragmentation index.
    0 // Fragmentation ID.
  );

  if (!m_header) {
    loge("MSH:Packet creation failed\n");

    return ESPCONN_ARG;
  }

  // Upwards packet, going out of the mesh to the mesh gateway.
  m_header->proto.d = MESH_ROUTE_UPWARDS;

  if (!espconn_mesh_set_usr_data(m_header, data_with_type, length)) {
    loge("MSH:Setting user data failed while sending\n");

    os_free(m_header);

    return ESPCONN_ARG;
  }

  /*
   * Just like mesh receive, the mesh send function will not send unless there is
   * a packet with valid mesh header and non-zero payload.
   */
  ret = espconn_mesh_sent(tfp_con_mesh.con, (uint8_t *)m_header, m_header->len);

  if(ret == ESPCONN_MEM) {
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
  else {
    logd("MSH:Send OK (T: %d)\n", packet_type);
  }

  os_free(m_header);

  return ret;
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_sent(void *arg) {
  uint8_t tfp_mesh_send_packet_len = 0;
  uint8_t tfp_mesh_send_packet[TFP_MAX_LENGTH];

  gw2mcsr.tx_count++;

  os_bzero(tfp_mesh_send_packet, sizeof(tfp_mesh_send_packet));

  /*
   * If the callback is for recently sent mesh hello packet then don't call the
   * callback function on TFP layer.
   */
  if(sent_mesh_hello) {
    sent_mesh_hello = false;
    logi("MSH:Sent mesh hello\n");

    return;
  }

  // If TFP mesh send buffer is empty then nothing to do.
  if(ringbuffer_is_empty(&tfp_mesh_send_rb)) {
    // Change the state of the socket to be ready to send.
    tfp_con_mesh.state = TFP_CON_STATE_OPEN;

    return;
  }

  // Send packet from the TFP send buffer and remove the packet from the buffer.

  // Read 5 bytes of the packet present in the buffer.
  if(ringbuffer_peak(&tfp_mesh_send_rb, tfp_mesh_send_packet, 5) != 5) {
    loge("MSH:Failed to peak for packet length on send buffer\n");

    return;
  }

  // Get the length field of the packet.
  tfp_mesh_send_packet_len = tfp_mesh_send_packet[4];

  /*
   * Now that the length of the packet is known try to get the whole packet
   * from the TFP mesh send buffer for sending.
   */
  if(ringbuffer_peak(&tfp_mesh_send_rb,
                     tfp_mesh_send_packet,
                     tfp_mesh_send_packet_len) != tfp_mesh_send_packet_len) {
    loge("MSH:Failed to peak for packet from send buffer\n");

    return;
  }

  logi("MSH:Sending from buffer...\n");

  tfp_mesh_send(tfp_con_mesh.con,
                tfp_mesh_send_packet,
                tfp_mesh_send_packet_len,
                MESH_PACKET_TFP);

  ringbuffer_remove(&tfp_mesh_send_rb, tfp_mesh_send_packet_len);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_connect(void *arg) {
  logi("MSH:Gateway connected\n");

  uint32_t i = 0;
  pkt_mesh_hello_t mesh_hello_pkt;
  struct espconn *sock = (struct espconn *)arg;

  if(!espconn_set_opt(sock, ESPCONN_NODELAY)) {
    loge("MSH:Error setting socket parameter, ESPCONN_NODELAY\n");
    espconn_mesh_disable(NULL);

    return;
  }

  // Register callbacks of the socket.
  if(espconn_regist_reconcb(sock, cb_tfp_mesh_reconnect) != 0) {
    loge("MSH:Failed to register reconnect callback");
    espconn_mesh_disable(NULL);

    return;
  }

  if(espconn_regist_disconcb(sock, cb_tfp_mesh_disconnect) != 0) {
    loge("MSH:Failed to register disconnect callback");
    espconn_mesh_disable(NULL);

    return;
  }

  if(espconn_regist_recvcb(sock, cb_tfp_mesh_receive) != 0) {
    loge("MSH:Failed to register receive callback\n");
    espconn_mesh_disable(NULL);

    return;
  }

  if(espconn_regist_sentcb(sock, cb_tfp_mesh_sent) != 0) {
    loge("MSH:Failed to register sent callback");
    espconn_mesh_disable(NULL);

    return;
  }

  init_tfp_con_mesh();
  tfp_mesh_send_buffer_clear();

  tfp_con_mesh.con = sock;

  ringbuffer_init(&tfp_mesh_send_rb,
                  tfp_mesh_send_rb_buffer,
                  sizeof(tfp_mesh_send_rb_buffer));

  // TODO: Should this root or not check be delayed ?
  if(espconn_mesh_is_root()) {
    logi("MSH:Sending mesh hello to gateway\n");

    os_bzero(&mesh_hello_pkt, sizeof(mesh_hello_pkt));

    mesh_hello_pkt.firmware_version[0] = FIRMWARE_VERSION_MAJOR;
    mesh_hello_pkt.firmware_version[1] = FIRMWARE_VERSION_MINOR;
    mesh_hello_pkt.firmware_version[2] = FIRMWARE_VERSION_REVISION;

    os_memcpy(&mesh_hello_pkt.prefix,
              configuration_current.mesh_ssid_prefix,
              sizeof(configuration_current.mesh_ssid_prefix));

    os_memcpy(&mesh_hello_pkt.group_id,
              configuration_current.mesh_group_id,
              sizeof(configuration_current.mesh_group_id));

    tfp_mesh_send(tfp_con_mesh.con,
                  (uint8_t *)&mesh_hello_pkt,
                  sizeof(pkt_mesh_hello_t),
                  MESH_PACKET_HELLO);

    sent_mesh_hello = true;
  }
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_new_node(void *mac) {
  /*
   * TODO: Cache connected child's stack for generating enumerate disconnect
   * callbacks when it is detected that the child is not connected anymore.
   */
  if (!mac) {
    return;
  }

  uint8_t *_mac = (uint8_t *)mac;

  logi("MSH:New node joined (M: %02X-%02X-%02X-%02X-%02X-%02X)\n",
       _mac[0], _mac[1], _mac[2],
       _mac[3], _mac[4], _mac[5]);
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_disconnect(void *arg) {
  logi("MSH:Connection to parent disconnected\n");

  brickd_disconnect(tfp_con_mesh.cid);
  tfp_mesh_send_buffer_clear();
  init_tfp_con_mesh();
  espconn_mesh_disable(cb_tfp_mesh_disable);
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

  // Create and connect the socket to the mesh gateway.
  tfp_mesh_open_connection();
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_disable(int8_t status) {
  logi("MSH:Mesh re-enabling...\n");

  espconn_mesh_enable(cb_tfp_mesh_enable, MESH_ONLINE);
}

/*
 * Callback only called if there is payload. Not called if the received packet
 * has only a mesh header but no payload.
 */
void ICACHE_FLASH_ATTR cb_tfp_mesh_receive(void *arg,
                                           char *pdata,
                                           unsigned short len) {
  uint32_t i = 0;
  uint8_t *pkt_tfp = NULL;
  uint16_t pkt_tfp_len = 0;
  struct mesh_header_format *m_header = NULL;

  gw2mcsr.rx_count++;

  for(i = 0; i < len; i++) {
		tfp_con_mesh.recv_buffer[tfp_con_mesh.recv_buffer_index] = pdata[i];

		if(tfp_con_mesh.recv_buffer_index == TFP_MESH_HEADER_LENGTH_INDEX) {
			tfp_con_mesh.recv_buffer_expected_length =
        (uint16_t)tfp_con_mesh.recv_buffer[TFP_MESH_HEADER_LENGTH_INDEX - 1];
		}

		tfp_con_mesh.recv_buffer_index++;

		if(tfp_con_mesh.recv_buffer_index == tfp_con_mesh.recv_buffer_expected_length) {
      m_header = (struct mesh_header_format *)tfp_con_mesh.recv_buffer;

      if(m_header == NULL) {
        loge("MSH:Receive failed, could not get mesh packet\n");

        goto ERROR_GETTING_PACKET;
      }

      if(m_header->proto.protocol != M_PROTO_BIN) {
        loge("MSH:Receive failed, packet protocol type is not binary");

        goto ERROR_GETTING_PACKET;
      }
      else {
        // Payload of a mesh packet is a TFP packet.
        pkt_tfp = NULL;

        if(!espconn_mesh_get_usr_data(m_header, &pkt_tfp, &pkt_tfp_len)) {
          loge("MSH:Receive failed, could not get user data\n");

          goto ERROR_GETTING_PACKET;
        }

        if(pkt_tfp == NULL) {
          loge("MSH:Receive failed, could not get TFP packet\n");

          goto ERROR_GETTING_PACKET;
        }
      }

      if(pkt_tfp[0] == MESH_PACKET_HELLO) {
        logi("MSH:Received hello packet\n");
      }
      else if(pkt_tfp[0] == MESH_PACKET_OLLEH) {
        logi("MSH:Received olleh packet\n");

        tfp_con_mesh.state = TFP_CON_STATE_OPEN;
      }
      else if(pkt_tfp[0] == MESH_PACKET_HB_RES) {
        logi("MSH:Received heartbeat response packet\n");
      }
      else if(pkt_tfp[0] == MESH_PACKET_HB_REQ) {
        logi("MSH:Received heartbeat request packet\n");
      }
      else if(pkt_tfp[0] == MESH_PACKET_TFP) {
        logi("MSH:Received TFP packet\n");

        if(!com_handle_message(pkt_tfp + 1, pkt_tfp_len - 1, tfp_con_mesh.cid)) {
    			brickd_route_from(pkt_tfp + 1, tfp_con_mesh.cid);
    			tfp_handle_packet(pkt_tfp + 1, pkt_tfp_len - 1);
    		}
      }
      else {
        logi("MSH:Received unknown packet\n");
      }

ERROR_GETTING_PACKET:
			tfp_con_mesh.recv_buffer_index = 0;
			tfp_con_mesh.recv_buffer_expected_length = TFP_MESH_MIN_LENGTH;
		}
	}
}

void ICACHE_FLASH_ATTR cb_tfp_mesh_reconnect(void *arg, sint8 error) {
  logi("MSH:Connection reconnect (E: %d)\n", error);

  cb_tfp_mesh_disconnect(arg);
}

bool ICACHE_FLASH_ATTR tfp_mesh_send_handler(const uint8_t *data,
                                             uint8_t length) {
  if(tfp_con_mesh.state == TFP_CON_STATE_OPEN) {
    os_memcpy(tfp_con_mesh.send_buffer, data, length);
    tfp_mesh_send(tfp_con_mesh.con, (uint8_t*)data, length, MESH_PACKET_TFP);

    return true;
  }
  else if(tfp_con_mesh.state == TFP_CON_STATE_SENDING){
    // Check if there is enough space in the send buffer.
    logd("MSH:Can't send now, buffering...\n");

    if(tfp_mesh_send_buffer_check(length)) {
      // Store the packet in the TFP mesh send buffer.
      for(uint32_t i = 0; i < length; i++) {
        if(!ringbuffer_add(&tfp_mesh_send_rb, data[i])) {
          loge("MSH:Buffering failed, could not add byte\n");

          return false;
        }
      }
      return true;
    }
    else {
      loge("MSH:Buffering failed, buffer full\n");

      return false;
    }
  }
  else {
    loge("MSH:Send failed, socket state\n");

    return false;
  }
}
