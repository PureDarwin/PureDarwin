/* swc: libswc/util.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SWC_UTIL_H
#define SWC_UTIL_H

#include "swc.h"

#include <pixman.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <wayland-util.h>

#define EXPORT __attribute__((visibility("default")))

#if ENABLE_DEBUG
#define MESSAGE_SOURCE fprintf(stderr, "[swc:%s:%d] ", __FILE__, __LINE__);
#else
#define MESSAGE_SOURCE
#endif

#define MESSAGE(type, format, ...)                                             \
	do {                                                                       \
		MESSAGE_SOURCE                                                         \
		fprintf(stderr, type ": " format, ##__VA_ARGS__);                      \
	} while (false)

#define WARNING(format, ...) MESSAGE("WARNING", format, ##__VA_ARGS__)
#define ERROR(format, ...) MESSAGE("ERROR", format, ##__VA_ARGS__)

#if ENABLE_DEBUG
#define DEBUG(format, ...) MESSAGE("DEBUG", format, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array)[0])

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct wl_resource;
struct wl_client;

void
remove_resource(struct wl_resource *resource);
void
destroy_resource(struct wl_client *client, struct wl_resource *resource);

static inline uint32_t
get_time(void)
{
	struct timeval timeval;

	gettimeofday(&timeval, NULL);
	return timeval.tv_sec * 1000 + timeval.tv_usec / 1000;
}

extern pixman_box32_t infinite_extents;

static inline bool
rectangle_contains_point(const struct swc_rectangle *rectangle,
                         int32_t x,
                         int32_t y)
{
	return x > rectangle->x && x < rectangle->x + rectangle->width &&
	       y > rectangle->y && y < rectangle->y + rectangle->height;
}

static inline bool
rectangle_overlap(const struct swc_rectangle *r1,
                  const struct swc_rectangle *r2)
{
	return (MAX(r1->x + r1->width, r2->x + r2->width) - MIN(r1->x, r2->x) <
	        r1->width + r2->width) &&
	       (MAX(r1->y + r1->height, r2->y + r2->height) - MIN(r1->y, r2->y) <
	        r1->height + r2->height);
}

static inline void
array_remove(struct wl_array *array, void *item, size_t size)
{
	size_t bytes =
	    array->size - ((intptr_t)item + size - (intptr_t)array->data);
	if (bytes > 0) {
		memmove(item, (void *)((intptr_t)item + size), bytes);
	}
	array->size -= size;
}

#endif
