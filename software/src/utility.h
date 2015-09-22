/* ESP8266
 * Copyright (C) 2015 Olaf Lüke <olaf@tinkerforge.com>
 *
 * utility.h: General utility functions
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


#ifndef UTILITY_H
#define UTILITY_H

#ifndef ABS
	#define ABS(a) (((a) < 0) ? (-(a)) : (a))
#endif
#ifndef MIN
	#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
	#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef BETWEEN
	#define BETWEEN(min, value, max)  (MIN(max, MAX(value, min)))
#endif

#define SCALE(val_a, min_a, max_a, min_b, max_b) \
	(((((val_a) - (min_a))*((max_b) - (min_b)))/((max_a) - (min_a))) + (min_b))

#endif
