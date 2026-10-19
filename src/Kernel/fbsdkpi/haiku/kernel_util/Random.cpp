/*
 * Copyright 2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */


#include <util/Random.h>

#include <OS.h>


static uint32	sFastLast	= 0;
static uint32	sLast		= 0;
static uint32	sSecureLast	= 0;

// MD4 helper definitions, based on RFC 1320
#define F(x, y, z)	(((x) & (y)) | (~(x) & (z)))
#define G(x, y, z)	(((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z)	((x) ^ (y) ^ (z))

#define STEP(f, a, b, c, d, xk, s)	\
	(a += f((b), (c), (d)) + (xk), a = (a << (s)) | (a >> (32 - (s))))


// MD4 based hash function. Simplified in order to improve performance.
static uint32
hash(uint32* data)
{
		const uint32 kMD4Round2 = 0x5a827999;
		const uint32 kMD4Round3 = 0x6ed9eba1;

		uint32 a = 0x67452301;
		uint32 b = 0xefcdab89;
		uint32 c = 0x98badcfe;
		uint32 d = 0x10325476;

		STEP(F, a, b, c, d, data[0], 3);
		STEP(F, d, a, b, c, data[1], 7);
		STEP(F, c, d, a, b, data[2], 11);
		STEP(F, b, c, d, a, data[3], 19);
		STEP(F, a, b, c, d, data[4], 3);
		STEP(F, d, a, b, c, data[5], 7);
		STEP(F, c, d, a, b, data[6], 11);
		STEP(F, b, c, d, a, data[7], 19);

		STEP(G, a, b, c, d, data[1] + kMD4Round2, 3);
		STEP(G, d, a, b, c, data[5] + kMD4Round2, 5);
		STEP(G, c, d, a, b, data[6] + kMD4Round2, 9);
		STEP(G, b, c, d, a, data[2] + kMD4Round2, 13);
		STEP(G, a, b, c, d, data[3] + kMD4Round2, 3);
		STEP(G, d, a, b, c, data[7] + kMD4Round2, 5);
		STEP(G, c, d, a, b, data[4] + kMD4Round2, 9);
		STEP(G, b, c, d, a, data[0] + kMD4Round2, 13);

		STEP(H, a, b, c, d, data[1] + kMD4Round3, 3);
		STEP(H, d, a, b, c, data[6] + kMD4Round3, 9);
		STEP(H, c, d, a, b, data[5] + kMD4Round3, 11);
		STEP(H, b, c, d, a, data[2] + kMD4Round3, 15);
		STEP(H, a, b, c, d, data[3] + kMD4Round3, 3);
		STEP(H, d, a, b, c, data[4] + kMD4Round3, 9);
		STEP(H, c, d, a, b, data[7] + kMD4Round3, 11);
		STEP(H, b, c, d, a, data[0] + kMD4Round3, 15);

		return b;
}


// In the following functions there are race conditions when many threads
// attempt to update static variable last. However, since such conflicts
// are non-deterministic it is not a big problem.


// A simple linear congruential generator
unsigned int
fast_random_value()
{
	if (sFastLast == 0)
		sFastLast = system_time();

	uint32 random = sFastLast * 1103515245 + 12345;
	sFastLast = random;
	return (random >> 16) & 0x7fff;
}


// Taken from "Random number generators: good ones are hard to find",
// Park and Miller, Communications of the ACM, vol. 31, no. 10,
// October 1988, p. 1195.
unsigned int
random_value()
{
	if (sLast == 0)
		sLast = system_time();

	uint32 hi = sLast / 127773;
	uint32 lo = sLast % 127773;

	int32 random = 16807 * lo - 2836 * hi;
	if (random <= 0)
		random += MAX_RANDOM_VALUE;
	sLast = random;
	return random % (MAX_RANDOM_VALUE + 1);
}


unsigned int
secure_random_value()
{
	static int32 count = 0;

	uint32 data[8];
	data[0] = atomic_add(&count, 1);
	data[1] = system_time();
	data[2] = find_thread(NULL);
	data[3] = smp_get_current_cpu();
	data[4] = real_time_clock();
	data[5] = sFastLast;
	data[6] = sLast;
	data[7] = sSecureLast;

	uint32 random = hash(data);
	sSecureLast = random;
	return random;
}

