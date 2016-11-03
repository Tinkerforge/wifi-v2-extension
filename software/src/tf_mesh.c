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

#include "osapi.h"
#include "tf_mesh.h"
#include "espmissingincludes.h"

void ICACHE_FLASH_ATTR cb_tf_mesh_new_node(void *mac) {
  if (!mac) {
    return;
  }

  os_printf("\n[+]MSH:NEW_NODE = %s\n", MAC2STR(((uint8_t *)mac)));
}

void ICACHE_FLASH_ATTR cb_tf_mesh_enable(int8_t status) {
  os_printf("\n[+]MSH:MSH_EN_CB,r=%d\n", status);
}
