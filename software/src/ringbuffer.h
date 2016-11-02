/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * ringbuffer.h: Ringbuffer implementation
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

#ifndef WIFI_RINGBUFFER_H
#define WIFI_RINGBUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "espmissingincludes.h"

typedef struct {
	uint32_t overflow_counter;
	uint32_t low_watermark;
	uint32_t start;
	uint32_t end;
	uint32_t buffer_length;
	uint8_t *buffer;
} Ringbuffer;

void ICACHE_FLASH_ATTR ringbuffer_init(Ringbuffer *rb, uint8_t *buffer, const uint32_t buffer_length);
uint32_t ICACHE_FLASH_ATTR ringbuffer_get_free(Ringbuffer *rb);
uint32_t ICACHE_FLASH_ATTR ringbuffer_get_used(Ringbuffer *rb);
bool ICACHE_FLASH_ATTR ringbuffer_is_empty(Ringbuffer *rb);
bool ICACHE_FLASH_ATTR ringbuffer_is_full(Ringbuffer *rb);
bool ICACHE_FLASH_ATTR ringbuffer_add(Ringbuffer *rb, uint8_t data);
void ICACHE_FLASH_ATTR ringbuffer_remove(Ringbuffer *rb, const uint16_t num);
bool ICACHE_FLASH_ATTR ringbuffer_get(Ringbuffer *rb, uint8_t *data);
uint32_t ICACHE_FLASH_ATTR ringbuffer_peak(Ringbuffer *rb, uint8_t *data, const uint32_t length);

#endif
