/*
 * Copyright (c) 2016 Lubos Dolezel
 *
 * This file is part of Darling CoreCrypto.
 *
 * Darling is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <corecrypto/cc.h>
#include <corecrypto/ccrng_system.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void cc_clear(size_t len, void *dst)
{
	volatile unsigned char *ptr = (volatile unsigned char*) dst;
	while (len--) {
		*ptr++ = 0;
	}
}

/* https://cryptocoding.net/index.php/Coding_rules#Avoid_branchings_controlled_by_secret_data */
void* cc_muxp(int s, const void *a, const void *b)
{
	uintptr_t mask = -(s != 0);
	uintptr_t ret = mask & (((uintptr_t)a) ^ ((uintptr_t)b));
	ret = ret ^ ((uintptr_t)b);
	return (void*) ret;
}

/* https://cryptocoding.net/index.php/Coding_rules#Compare_secret_strings_in_constant_time */
int cc_cmp_safe(size_t size, const void* a, const void* b)
{
	if ( size == 0) {
		return 1;
	}
	const unsigned char *_a = (const unsigned char *) a;
	const unsigned char *_b = (const unsigned char *) b;
	unsigned char result = 0;
	size_t i;

	for (i = 0; i < size; i++)
	{
		result |=  (unsigned) _a[i] ^ _b[i];
	}

	return result ? 1 : 0;
}

