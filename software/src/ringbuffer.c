/* WIFI Extension 2.0
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * ringbuffer.c: Ringbuffer implementation
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

#include "ringbuffer.h"

#include "osapi.h"
#include "logging.h"

void ICACHE_FLASH_ATTR ringbuffer_init(Ringbuffer *rb, uint8_t *buffer, uint32_t buffer_length) {
	rb->overflow_counter = 0;
	rb->low_watermark = 0;
	rb->start = 0;
	rb->end = 0;
	rb->buffer_length = buffer_length;
	rb->buffer = buffer;
}

uint32_t ICACHE_FLASH_ATTR ringbuffer_get_used(Ringbuffer *rb) {
	if(rb->end < rb->start) {
		return rb->buffer_length + rb->end - rb->start;
	}

	return rb->end - rb->start;
}

uint32_t ICACHE_FLASH_ATTR ringbuffer_get_free(Ringbuffer *rb) {
	const uint32_t free = rb->buffer_length - ringbuffer_get_used(rb);

	if(free < rb->low_watermark) {
		rb->low_watermark = free;
	}

	return free;
}

bool ICACHE_FLASH_ATTR ringbuffer_is_empty(Ringbuffer *rb) {
	return rb->start == rb->end;
}

bool ICACHE_FLASH_ATTR ringbuffer_is_full(Ringbuffer *rb) {
	return ringbuffer_get_free(rb) < 2;
}

bool ICACHE_FLASH_ATTR ringbuffer_add(Ringbuffer *rb, uint8_t data) {
	if(ringbuffer_is_full(rb)) {
		logw("Ringbuffer Overflow!\n");
		rb->overflow_counter++;
		return false;
	}

	rb->buffer[rb->end] = data;
	rb->end++;
	if(rb->end >= rb->buffer_length) {
		rb->end = 0;
	}
	return true;
}

bool ICACHE_FLASH_ATTR ringbuffer_get(Ringbuffer *rb, uint8_t *data) {
	if(ringbuffer_is_empty(rb)) {
		return false;
	}

	*data = rb->buffer[rb->start];
	rb->start++;
	if(rb->start >= rb->buffer_length) {
		rb->start = 0;
	}
	return true;
}

uint32_t ICACHE_FLASH_ATTR ringbuffer_peak(Ringbuffer *rb, uint8_t *data, uint32_t length) {
	uint32_t p = rb->start;
	uint32_t peak_length = length;
	uint32_t used = ringbuffer_get_used(rb);
	if(peak_length > used) {
		peak_length = used;
	}

	for(uint32_t i = 0; i < peak_length; i++) {
		data[i] = rb->buffer[p];
		p++;
		if(p >= rb->buffer_length) {
			p = 0;
		}
	}

	return peak_length;
}

